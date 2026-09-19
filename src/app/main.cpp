/// @file src/app/main.cpp
/// @brief 应用进程入口；建立Qt事件循环，处理教学/CSV启动和离屏截图参数。
#include "ui_widgets/main_window.h"
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QDir>
#include <QTabWidget>
/// 应用进程入口；建立Qt事件循环，处理教学/CSV启动和离屏截图参数。
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("GPUView");
    app.setApplicationVersion("0.3.0");
    // Windows离屏平台不一定自动枚举系统字体；只读取本机字体，不打包分发字体文件。
#ifdef Q_OS_WIN
    const auto fontPath = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("Fonts/msyh.ttc");
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    const auto families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty()) app.setFont(QFont(families.front(), 10));
#endif
    gpuview::MainWindow window;
    const auto args = app.arguments();
    const int openFile = args.indexOf("--open");
    const QString input = openFile >= 0 && openFile + 1 < args.size() ? args[openFile + 1] : QString();
    const int capture = args.indexOf("--screenshot");
    if (capture >= 0 && capture + 1 < args.size()) {
        const auto path = args[capture + 1];
        // 快照就绪后安排截图；窗口作为连接上下文，销毁时自动断开，捕获路径按值保存。
        QObject::connect(window.controller(), &gpuview::SessionController::snapshotReady, &window, [&window, path, analysis = args.contains("--analysis")] {
            // 等待布局与异步统计有机会刷新后截图；成功/失败用退出码反馈，不将等待时长作为性能测量。
            QTimer::singleShot(1000, &window, [&window, path, analysis] {
                if (analysis) window.findChild<QTabWidget*>("detailTabs")->setCurrentIndex(1);
                const bool saved = window.grab().save(path);
                QCoreApplication::exit(saved ? 0 : 2);
            });
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
