#include "ui_widgets/timeline_widget.h"
#include "adapters/synthetic_source.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QDir>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif
using namespace gpuview;
int main(int argc,char** argv) {
    QApplication app(argc,argv);
#ifdef _WIN32
    const int fontId=QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("SystemRoot","C:/Windows")).filePath("Fonts/msyh.ttc"));
    const auto families=QFontDatabase::applicationFontFamilies(fontId);
    if(!families.isEmpty()) app.setFont(QFont(families.front(),10));
#endif
    const auto args=app.arguments();
    if(args.size()<3) return 1;
    bool valid=false; const auto count=args[2].toULongLong(&valid); if(!valid || count>1000000 || count==0) return 2;
    QFile file(args[1]); if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) return 3;
    QTextStream out(&file);
    QElapsedTimer clock; clock.start(); const auto snapshot=generateTrace(std::size_t(count),1); const double loadMs=clock.nsecsElapsed()/1e6;
    TimelineWidget widget; widget.resize(1000,600); widget.setSnapshot(snapshot); widget.show(); app.processEvents();
    out<<"events,run,cache,workload,sample,input_to_cpu_paint_ms,paint_ms,load_ms,peak_working_set_bytes,primitives\n";
    for(int run=-1;run<3;++run) for(int mode=0;mode<2;++mode) {
        const bool cached=(mode+(run+1))%2==0; widget.setCacheEnabled(cached);
        for(const QString workload:{QString("zoom"),QString("pan"),QString("hover")}) {
            widget.resetViewport();
            if(workload=="pan") widget.showRange({snapshot->bounds.end/4,snapshot->bounds.end/2});
            widget.repaint();
            for(int sample=0;sample<120;++sample) {
                clock.restart();
                if(workload=="zoom") {
                    QWheelEvent e(QPointF(500,100),QPointF(500,100),QPoint(),QPoint(0,sample%2? -120:120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                    QApplication::sendEvent(&widget,&e);
                } else if(workload=="pan") {
                    const auto start=snapshot->bounds.end/4 + TimeNs(sample%20)*snapshot->bounds.end/1000;
                    widget.showRange({start,start+snapshot->bounds.end/4});
                } else {
                    QMouseEvent e(QEvent::MouseMove,QPointF(200+sample,80),QPointF(200+sample,80),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(&widget,&e);
                }
                widget.repaint();
                const double total=clock.nsecsElapsed()/1e6;
                std::size_t peak=0;
#ifdef _WIN32
                PROCESS_MEMORY_COUNTERS memory{};
                if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory))) peak=memory.PeakWorkingSetSize;
#endif
                if(run>=0) out<<count<<','<<run<<','<<(cached?"on":"off")<<','<<workload<<','<<sample<<','<<total<<','<<widget.lastPaintMs()<<','<<loadMs<<','<<qulonglong(peak)<<','<<qulonglong(widget.primitiveCount())<<'\n';
            }
        }
    }
    return 0;
}
