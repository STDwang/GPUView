#include "ui_widgets/main_window.h"
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QDir>
#include <QTabWidget>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("GPUView");
    app.setApplicationVersion("0.2.0");
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
        QObject::connect(window.controller(), &gpuview::SessionController::snapshotReady, &window, [&window, path, analysis = args.contains("--analysis")] {
            QTimer::singleShot(1000, &window, [&window, path, analysis] {
                if (analysis) window.findChild<QTabWidget*>("detailTabs")->setCurrentIndex(1);
                const bool saved = window.grab().save(path);
                QCoreApplication::exit(saved ? 0 : 2);
            });
        });
        QTimer::singleShot(0, &window, [&window, input] { if (input.isEmpty()) window.controller()->requestSynthetic(1000000); else window.controller()->requestFile(input); });
        QTimer::singleShot(30000, &app, [] { QCoreApplication::exit(3); });
    }
    if (capture < 0 && !input.isEmpty()) QTimer::singleShot(0, &window, [&window, input] { window.controller()->requestFile(input); });
    window.show();
    return app.exec();
}
