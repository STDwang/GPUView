/// @file src/app/main.cpp
/// @brief 应用进程入口；建立Qt事件循环，处理教学/CSV启动和离屏截图参数。
#include "ui_widgets/main_window.h"
#include "ui_widgets/event_explorer.h"
#include "ui_widgets/frame_details_panel.h"
#include "ui_widgets/theme.h"
#include <QApplication>
#include <QTimer>
#include <QTabWidget>
/// 应用进程入口；建立Qt事件循环，处理教学/CSV启动和离屏截图参数。
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("GPUView");
    app.setApplicationVersion("0.4.0");
    gpuview::configureApplicationTheme(app);
    gpuview::MainWindow window;
    const auto args = app.arguments();
    const int openFile = args.indexOf("--open");
    const QString input = openFile >= 0 && openFile + 1 < args.size() ? args[openFile + 1] : QString();
    const int capture = args.indexOf("--screenshot");
    if (capture >= 0 && capture + 1 < args.size()) {
        const auto path = args[capture + 1];
        // 等待真实分析结果，不能用固定1秒延迟假定百万事件已完成。
        QObject::connect(window.controller(), &gpuview::SessionController::snapshotReady, &window, [&window, path, analysis = args.contains("--analysis")] {
            auto* readyTimer=new QTimer(&window); readyTimer->setInterval(100);
            // 查询只读发布状态；零条结果也算完成，统计失败由全局超时返回非零退出码。
            QObject::connect(readyTimer,&QTimer::timeout,&window,[&window,path,analysis,readyTimer] {
                const auto snapshot=window.controller()->snapshot();
                const auto events=window.findChild<gpuview::EventAnalysisModel*>("eventResultsModel")->result();
                if(!events || events->source!=snapshot) return;
                const auto frames=window.findChild<gpuview::FrameTableModel*>()->analysis();
                if(snapshot->frames && (!frames || frames->source!=snapshot)) return;
                readyTimer->stop();
                if (analysis) window.findChild<QTabWidget*>("detailTabs")->setCurrentIndex(1);
                const bool saved = window.grab().save(path);
                QCoreApplication::exit(saved ? 0 : 2);
            });
            readyTimer->start();
        });
        // 事件循环启动后才发起加载；指定输入则导入，否则生成百万教学事件。
        QTimer::singleShot(0, &window, [&window, input] { if (input.isEmpty()) window.controller()->requestSynthetic(1000000); else window.controller()->requestFile(input); });
        // 截图超时返回非零码，避免自动验证永久挂起。
        QTimer::singleShot(30000, &app, [] { QCoreApplication::exit(3); });
    }
    // 普通文件启动也延迟到事件循环执行，确保接收方和窗口布局已建立。
    if (capture < 0 && !input.isEmpty()) QTimer::singleShot(0, &window, [&window, input] { window.controller()->requestFile(input); });
    window.show();
    return app.exec();
}
