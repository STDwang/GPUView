/// @file src/adapters/analysis_export.cpp
/// @brief 将冻结帧分析序列化为CSV/Markdown，并以原子文件替换保证失败或取消不破坏旧报告。
#include "adapters/analysis_export.h"
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>
#include <QLocale>
namespace gpuview {
namespace {
/// 转义CSV引号/换行并处理公式起始字符，避免表格软件将元数据作为公式执行。
QString csv(QString value) {
    // 将来源/备注作为文本导出，避免表格软件把外部字符串解释为公式。
    const auto trimmed=value.trimmed();
    if(!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())) value.prepend(QChar(0x27));
    value.replace('"',"\"\""); return '"'+value+'"'; }
/// 转义Markdown表格与HTML特殊字符，避免备注破坏报告结构。
QString markdown(QString value) {
    value.replace('&',"&amp;"); value.replace('<',"&lt;"); value.replace('>',"&gt;");
    value.replace('|',"&#124;"); value.replace('\r'," "); value.replace('\n',"<br>");
    value.replace('`',"&#96;"); value.replace('[',"&#91;"); value.replace(']',"&#93;");
    return value;
}
}
/// 把冻结分析写入已打开设备；UTF-8输出，支持取消/进度，写失败抛异常；不负责提交目标文件。
void writeAnalysis(QIODevice& output, const FrameAnalysis& analysis, ExportFormat format,
    const QString& notes, const CancelFlag& cancel, const Progress& progress) {
    if(!analysis.source || !analysis.source->frames) throw std::invalid_argument("frame analysis required");
    QTextStream stream(&output); stream.setEncoding(QStringConverter::Utf8); stream.setLocale(QLocale::c());
    const auto& source=*analysis.source; const auto& stats=analysis.summary;
    const auto track=analysis.tracks.empty() ? QStringLiteral("none") : QString::fromStdString(source.tracks[analysis.tracks.front()].name);
    QList<QPair<QString,QString>> fields{
        {"csv_text_policy","formula-leading metadata values are prefixed with apostrophe"},{"report_schema","GPUView-analysis-v1"},{"application_version","0.3.0"},
        {"source_sha256",QString::fromStdString(source.input.sha256.empty()?"unavailable":source.input.sha256)},
        {"source",QString::fromStdString(source.source)},{"synthetic",source.synthetic?"true":"false"},
        {"source_records",QString::number(qulonglong(source.input.records))},
        {"source_rejected",QString::number(qulonglong(source.input.rejected))},
        {"snapshot_version",QString::number(qulonglong(source.version))},{"group",track},
        {"range_begin_ns",QString::number(analysis.range.begin)},{"range_end_ns",QString::number(analysis.range.end)},
        {"selection_rule","Present timestamp in [begin,end); full MsBetweenPresents; all valid records, no Dropped filter"},
        {"long_frame_rule","v1: >max(2*1e9/60 ns, 2*median(previous up to 120 valid intervals)); <30 history uses budget only"},
        {"percentile_rule","nearest-rank"},{"sort_column",QString::number(int(analysis.sort))},
        {"sort_descending",analysis.descending?"true":"false"},{"count",QString::number(qulonglong(stats.count))},
        {"sum_ms",QString::number(stats.sumMs,'g',17)},
        {"mean_ms",stats.count?QString::number(stats.meanMs,'g',17):"N/A"},
        {"p50_ms",stats.count?QString::number(stats.p50Ms,'g',17):"N/A"},
        {"p95_ms",stats.count?QString::number(stats.p95Ms,'g',17):"N/A"},
        {"p99_ms",stats.count?QString::number(stats.p99Ms,'g',17):"N/A"},
        {"interval_fps",stats.count?QString::number(1000/stats.meanMs,'g',17):"N/A"},
        {"long_frames",QString::number(qulonglong(stats.longFrames))},{"user_notes",notes},
        {"limitations","Present interval is not GPU execution duration or displayed FPS; full source optional fields are not retained"}
    };
    for(std::size_t i=0;i<source.warnings.size();++i) fields.append({QString("warning_%1").arg(i+1),QString::fromStdString(source.warnings[i])});
    if(format==ExportFormat::Markdown) {
        stream<<QStringLiteral("# GPUView 帧分析报告\n\n| 字段 | 值 |\n|---|---|\n");
        for(const auto& field:fields) { checkCancelled(cancel); stream<<"| "<<markdown(field.first)<<" | "<<markdown(field.second)<<" |\n"; }
    } else {
        // 统一列数的带类型CSV：metadata行保存来源/口径，frame行保存全部所选帧，不另写可能失配的sidecar。
        stream<<"record_type,key,value,event_id,present_ns,frame_interval_ns,long_frame\n";
        for(const auto& field:fields) { checkCancelled(cancel); stream<<"metadata,"<<csv(field.first)<<','<<csv(field.second)<<",,,,\n"; }
        if(format==ExportFormat::FramesCsv) for(std::size_t row=0;row<analysis.rows.size();++row) {
            if(row%1024==0) { if(progress) progress(int(99*row/std::max<std::size_t>(1,analysis.rows.size()))); checkCancelled(cancel); stream.flush(); if(stream.status()!=QTextStream::Ok) throw std::runtime_error("Export write failed"); }
            const auto& e=analysis.event(row);
            stream<<"frame,,,"<<qulonglong(e.id)<<','<<e.start<<','<<e.duration<<','<<(analysis.rows[row].longFrame?1:0)<<'\n';
        }
    }
    if(progress) progress(99);
    checkCancelled(cancel); stream.flush();
    if(stream.status()!=QTextStream::Ok) throw std::runtime_error("Export write failed");
}
/// 原子保存分析到path；commit前取消或失败保留旧文件，提交成功后报告才可见。
void saveAnalysis(const QString& path,const FrameAnalysis& analysis,ExportFormat format,
    const QString& notes,const CancelFlag& cancel,const Progress& progress) {
    checkCancelled(cancel);
    QSaveFile file(path); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)) throw std::runtime_error(file.errorString().toStdString());
    try { writeAnalysis(file,analysis,format,notes,cancel,progress); checkCancelled(cancel); }
    catch(...) { file.cancelWriting(); throw; }
    // 原子替换是提交点；取消/异常发生在此之前时，旧文件仍然完整。
    if(!file.commit()) throw std::runtime_error(file.errorString().toStdString());
    if(progress) progress(100);
}
}
