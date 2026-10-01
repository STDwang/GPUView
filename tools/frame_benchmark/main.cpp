/// @file tools/frame_benchmark/main.cpp
/// @brief 帧分析/完整表格基准及公开样本报告验证；百万数据是合成帧，内存是进程累计峰值。
#include "core/frame_analysis.h"
#include "adapters/presentmon_csv.h"
#include "adapters/analysis_export.h"
#include "ui_widgets/frame_table_model.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTableView>
#include <QHeaderView>
#include <QFontDatabase>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif
using namespace gpuview;
/// 同进程交替测量冷算/复用排序；构造数据、旧结果和逐行一致性检查均在计时之外。
int benchmarkFrameSort(const QString& path) {
    QFile output(path); if(!output.open(QIODevice::WriteOnly)) return 2; QTextStream stream(&output);
    stream<<"events,sort,run,mode,analysis_ms,rows\n";
    for(int count:{100000,1000000}) {
        std::vector<Event> events; events.reserve(std::size_t(count));
        for(int i=0;i<count;++i) events.push_back({std::uint64_t(i+1),TimeNs(i)*16666667,i%60==59?70000000:16666667,0,0});
        const auto source=buildStore(std::move(events),{"synthetic frame sort"},{"synthetic"},1,{}, {},true,true);
        const auto previous=analyzeFrames(source,source->bounds,{0});
        for(auto sort:{FrameSort::Duration,FrameSort::LongFrame}) for(int run=-1;run<5;++run) {
            FrameAnalysisPtr results[2]; double elapsed[2]{};
            for(int step=0;step<2;++step) {
                const int mode=(step+(run+1)%2)%2; QElapsedTimer timer; timer.start();
                results[mode]=analyzeFrames(source,source->bounds,{0},sort,true,{},mode?previous:FrameAnalysisPtr{});
                elapsed[mode]=timer.nsecsElapsed()/1e6;
            }
            // 不能以更少的行或近似统计换取提速；测试也覆盖全部摘要字段与缓存失效条件。
            if(results[0]->rows.size()!=std::size_t(count) || results[0]->rows.size()!=results[1]->rows.size() ||
                results[0]->idRows!=results[1]->idRows || results[0]->heatOverview!=results[1]->heatOverview ||
                results[0]->summary.p99Ms!=results[1]->summary.p99Ms || !results[1]->reusedSelection) return 3;
            for(std::size_t i=0;i<results[0]->rows.size();++i)
                if(results[0]->event(i).id!=results[1]->event(i).id || results[0]->rows[i].longFrame!=results[1]->rows[i].longFrame) return 3;
            if(run>=0) for(int mode=0;mode<2;++mode)
                stream<<count<<','<<(sort==FrameSort::Duration?"duration":"long_frame")<<','<<run<<','<<(mode?"reuse":"cold")<<','<<elapsed[mode]<<','<<count<<'\n';
            stream.flush();
        }
    }
    return 0;
}
/// 帧分析/完整表格基准及公开样本报告验证；百万数据是合成帧，内存是进程累计峰值。
int main(int argc,char** argv) {
    QApplication app(argc,argv); const auto args=app.arguments();
    if(args.size()==3 && args[1]=="--sort") {
        try { return benchmarkFrameSort(args[2]); }
        catch(const std::exception& error) { QTextStream(stderr)<<error.what()<<'\n'; return 4; }
    }
    if(args.size()<4) return 1;
#ifdef _WIN32
    const auto id=QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("SystemRoot","C:/Windows")).filePath("Fonts/msyh.ttc"));
    const auto families=QFontDatabase::applicationFontFamilies(id); if(!families.isEmpty()) app.setFont(QFont(families.front(),10));
#endif
    try {
        const auto real=loadPresentMon(std::filesystem::path(args[1].toStdWString()),1);
        const auto report=analyzeFrames(real,real->bounds,{0},FrameSort::Duration,true);
        QDir().mkpath(args[2]);
        const auto notes=QStringLiteral("可复核示例：逗号, 引号\" 与换行\n仅使用仓库公开样本");
        saveAnalysis(QDir(args[2]).filePath("summary.csv"),*report,ExportFormat::SummaryCsv,notes);
        saveAnalysis(QDir(args[2]).filePath("report.md"),*report,ExportFormat::Markdown,notes);
        saveAnalysis(QDir(args[2]).filePath("frames.csv"),*report,ExportFormat::FramesCsv,notes);
        QFile output(args[3]); if(!output.open(QIODevice::WriteOnly)) return 2; QTextStream stream(&output);
        stream<<"events,run,analysis_ms,model_reset_ms,first_table_paint_ms,rows,peak_working_set_bytes\n";
        for(const int count:{100000,1000000}) {
            std::vector<Event> events; events.reserve(std::size_t(count));
            for(int i=0;i<count;++i) events.push_back({std::uint64_t(i+1),TimeNs(i)*16666667, i%60==59?70000000:16666667,0,0});
            const auto source=buildStore(std::move(events),{"synthetic frame benchmark"},{"synthetic frame"},1,{}, {},true,true,"synthetic deterministic frame benchmark");
            FrameTableModel model; QTableView table; table.setModel(&model); table.resize(1000,260);
            table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed); table.show(); app.processEvents();
            for(int run=-1;run<3;++run) {
                QElapsedTimer timer; timer.start(); auto analysis=analyzeFrames(source,source->bounds,{0},FrameSort::Duration,true);
                const double analysisMs=timer.nsecsElapsed()/1e6;
                timer.restart(); model.setAnalysis(analysis); const double resetMs=timer.nsecsElapsed()/1e6;
                timer.restart(); const auto image=table.grab(); const double paintMs=timer.nsecsElapsed()/1e6;
                if(image.isNull() || model.rowCount()!=count || model.eventAt(0)->duration!=70000000) return 3;
                std::size_t peak=0;
#ifdef _WIN32
                PROCESS_MEMORY_COUNTERS counters{}; if(GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))) peak=counters.PeakWorkingSetSize;
#endif
                if(run>=0) stream<<count<<','<<run<<','<<analysisMs<<','<<resetMs<<','<<paintMs<<','<<model.rowCount()<<','<<qulonglong(peak)<<'\n';
            }
        }
    } catch(const std::exception& error) { QTextStream(stderr)<<error.what()<<'\n'; return 4; }
    return 0;
}
