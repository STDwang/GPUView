#include "ui_widgets/main_window.h"
#include <QDockWidget>
#include <QLabel>
#include <QProgressBar>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
namespace gpuview {
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("GPUView · 性能可视化学习工作台"));
    resize(1440, 920);
    setStyleSheet(QStringLiteral("QMainWindow,QWidget{background:#101923;color:#d7e4ee;} QToolBar{background:#1b2a38;padding:8px;spacing:10px;} QToolButton{padding:7px;border:1px solid #385367;border-radius:4px;} QToolButton:hover{background:#2c4f63;} QDockWidget::title{background:#203343;padding:7px;} QTextBrowser{background:#162330;border:0;padding:12px;} QStatusBar{background:#172532;}"));
    auto* toolbar = addToolBar(QStringLiteral("数据与视图"));
    toolbar->setMovable(false);
    toolbar->addAction(QStringLiteral("10万 教学事件"), this, [this] { controller_.requestSynthetic(100000); });
    toolbar->addAction(QStringLiteral("100万 教学事件"), this, [this] { controller_.requestSynthetic(1000000); });
    auto* cancel = toolbar->addAction(QStringLiteral("取消构建"), &controller_, &SessionController::cancel);
    cancel->setEnabled(false);
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("全览 [Home]"), this, [this] { timeline_->resetViewport(); });
    auto* progress = new QProgressBar;
    progress->setMaximumWidth(140);
    progress->setRange(0, 100);
    progress->setValue(0);
    toolbar->addWidget(progress);
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    auto* heading = new QLabel(QStringLiteral("  GPUView / TRACE LAB    ·    教学模拟数据    ·    seed 42    ·    64 tracks"));
    heading->setMinimumHeight(44);
    heading->setStyleSheet(QStringLiteral("color:#55dabb;font-size:15px;font-weight:600;"));
    layout->addWidget(heading);
    timeline_ = new TimelineWidget;
    auto* trackLayout = new QHBoxLayout;
    trackLayout->addWidget(timeline_, 1);
    auto* scroll = new QScrollBar(Qt::Vertical);
    scroll->setObjectName(QStringLiteral("trackScrollBar"));
    // 显式区分滑槽和滑块，避免全局深色背景覆盖原生样式后只剩轮廓。
    scroll->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical{background:#0b121a;width:20px;margin:0;border:1px solid #35495a;border-radius:6px;}"
        "QScrollBar::handle:vertical{background:#7896ad;min-height:40px;margin:2px;border-radius:5px;}"
        "QScrollBar::handle:vertical:hover{background:#a5c9e0;}"
        "QScrollBar::handle:vertical:pressed{background:#55dabb;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;border:0;background:transparent;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"));
    scroll->setRange(0, 0);
    scroll->setToolTip(QStringLiteral("切换首个可见轨道"));
    trackLayout->addWidget(scroll);
    layout->addLayout(trackLayout, 1);
    connect(scroll, &QScrollBar::valueChanged, timeline_, &TimelineWidget::setFirstTrack);
    connect(timeline_, &TimelineWidget::trackScrollChanged, scroll, [scroll](int first, int maximum, int pageStep) {
        // 视图提供范围，窗口负责控件绑定；阻断回传以免更新范围时递归改动视图。
        const QSignalBlocker blocker(scroll);
        scroll->setRange(0, maximum);
        scroll->setPageStep(pageStep);
        scroll->setValue(first);
    });
    setCentralWidget(central);
    auto* detailDock = new QDockWidget(QStringLiteral("事件 / 面试讲解"), this);
    auto* detail = new QTextBrowser;
    detail->setMinimumWidth(280);
    detail->setText(QStringLiteral("学习版 0.1\n\n当前演示：\n• 百万事件紧凑存储\n• 子树最大结束时间索引\n• 可见区域裁剪与LOD\n• 后台构建与协作取消\n• 不可变快照与版本校验\n\n操作：\n滚轮缩放，中键拖动平移\n左键框选，点击查看原始事件\nHome全览，Esc清除选区\n\n限制：当前没有真实数据导入，\n密集区概览是近似显示。\n正式帧统计和遥测在后续任务实现。"));
    detailDock->setWidget(detail);
    addDockWidget(Qt::RightDockWidgetArea, detailDock);
    connect(timeline_, &TimelineWidget::eventPicked, this, [detail](qulonglong, const QString& text) { detail->setPlainText(text); });
    connect(timeline_, &TimelineWidget::rangeSelected, this, [detail](qint64 a, qint64 b) {
        detail->setPlainText(QStringLiteral("选区 [%1, %2) ms\n跨度 %3 ms\n\n选区统计模块待实现；不以概览像素计算统计。")
            .arg(double(a) / 1e6, 0, 'f', 3).arg(double(b) / 1e6, 0, 'f', 3).arg(double(b - a) / 1e6, 0, 'f', 3));
    });
    auto* diagnostics = new QLabel;
    statusBar()->addPermanentWidget(diagnostics, 1);
    connect(timeline_, &TimelineWidget::diagnosticsChanged, diagnostics, &QLabel::setText);
    connect(&controller_, &SessionController::progressChanged, progress, &QProgressBar::setValue);
    connect(&controller_, &SessionController::message, this, [this](const QString& text) { statusBar()->showMessage(text, 5000); });
    connect(&controller_, &SessionController::busyChanged, this, [cancel, progress](bool busy) {
        cancel->setEnabled(busy);
        if (!busy) progress->setValue(0);
    });
    connect(&controller_, &SessionController::snapshotReady, this, [this, scroll] {
        timeline_->setSnapshot(controller_.snapshot());
        scroll->setValue(0);
        statusBar()->showMessage(QStringLiteral("已载入 %1 个教学事件").arg(controller_.snapshot()->eventCount), 4000);
    });
}
}
