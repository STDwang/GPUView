/// @file src/ui_widgets/main_window.cpp
/// @brief Widgets应用组装入口；连接导航、时间轴、统计与导出，不直接解析文件或跑重计算。
#include "ui_widgets/main_window.h"
#include "ui_widgets/frame_time_widget.h"
#include "ui_widgets/frame_details_panel.h"
#include "ui_widgets/frame_heatmap.h"
#include <QFileInfo>
#include <QDockWidget>
#include <QLabel>
#include <QProgressBar>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenuBar>
namespace gpuview {
/// 组装Dock、工具栏及连接；只协调界面，解析与大规模计算交给后台模块。
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("GPUView · 性能分析与学习工作台")); resize(1440, 920);
    setStyleSheet(QStringLiteral("QMainWindow,QWidget{background:#101923;color:#d7e4ee;} QToolBar{background:#1b2a38;padding:6px;spacing:8px;} QToolButton,QPushButton{padding:6px;border:1px solid #385367;border-radius:4px;} QToolButton:hover,QPushButton:hover{background:#2c4f63;} QDockWidget::title{background:#203343;padding:7px;} QTextBrowser{background:#162330;border:0;padding:8px;} QStatusBar{background:#172532;} QLineEdit,QComboBox{padding:5px;border:1px solid #385367;} QTreeWidget,QTableWidget,QTableView{alternate-background-color:#182431;} QTabBar::tab{padding:8px;} QTabBar::tab:selected{background:#2c4f63;} QScrollBar:vertical{background:#0b121a;width:20px;margin:0;border:1px solid #35495a;border-radius:6px;} QScrollBar::handle:vertical{background:#7896ad;min-height:40px;margin:2px;border-radius:5px;} QScrollBar::handle:vertical:hover{background:#a5c9e0;} QScrollBar::handle:vertical:pressed{background:#55dabb;} QScrollBar::handle:vertical:disabled{background:#243442;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;border:0;background:transparent;} QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"));
    auto* toolbar = addToolBar(QStringLiteral("数据与视图")); toolbar->setMovable(false);
    // 文件对话框只采集路径，真正读取交给会话控制器，GUI不解析CSV。
    toolbar->addAction(QStringLiteral("导入 CSV"), this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("PresentMon v1 CSV（TimeInSeconds + MsBetweenPresents）"), {}, "CSV (*.csv)");
        if (!path.isEmpty()) controller_.requestFile(path);
    });
    // 发起固定规模教学数据请求，用于快速交互演示。
    toolbar->addAction(QStringLiteral("10万 教学事件"), this, [this] { controller_.requestSynthetic(100000); });
    // 发起百万教学数据请求，验证索引/LOD与后台取消边界。
    toolbar->addAction(QStringLiteral("100万 教学事件"), this, [this] { controller_.requestSynthetic(1000000); });
    auto* cancel = toolbar->addAction(QStringLiteral("取消加载"), &controller_, &SessionController::cancel); cancel->setEnabled(false);
    toolbar->addSeparator();
    // 复用时间轴统一全览入口，保持按钮与Home按键行为一致。
    toolbar->addAction(QStringLiteral("全览 [Home]"), this, [this] { timeline_->resetViewport(); });
    // 缩放已有选择，不另建一套范围计算逻辑。
    toolbar->addAction(QStringLiteral("缩放到选区"), this, [this] { timeline_->zoomSelection(); });
    // 调用有限视图历史，恢复上一次时间范围。
    toolbar->addAction(QStringLiteral("上一视图"), this, [this] { timeline_->previousView(); });
    auto* progress = new QProgressBar; progress->setMaximumWidth(100); progress->setRange(0,100); progress->setValue(0); toolbar->addWidget(progress);
    auto* central = new QWidget; auto* layout = new QVBoxLayout(central);
    auto* heading = new QLabel(QStringLiteral("GPUView / TRACE LAB · 尚未加载数据")); heading->setWordWrap(true);
    heading->setStyleSheet(QStringLiteral("color:#55dabb;padding:8px;font-weight:600;")); layout->addWidget(heading);
    auto* legend = new QLabel(QStringLiteral("蓝：CPU教学轨道  绿：GPU教学轨道  金框：选中事件  |  密集区亮度随桶计数增加（对数刻度，非利用率）"));
    legend->setTextFormat(Qt::RichText); legend->setWordWrap(true); layout->addWidget(legend);
    auto* frameChart = new FrameTimeWidget; frameChart->hide();
    // 帧图与时间轴共享网格列，避免滚动条样式/DPI造成时间坐标错位。
    auto* plotsLayout = new QGridLayout; plotsLayout->addWidget(frameChart,0,0);
    auto* heatmap = new FrameHeatmap; heatmap->hide(); plotsLayout->addWidget(heatmap,1,0);
    timeline_ = new TimelineWidget;
    // 将热力图选择的纳秒范围交给时间轴，统一驱动选区和统计联动。
    connect(heatmap,&FrameHeatmap::rangePicked,this,[this](qint64 a,qint64 b) { timeline_->selectRange({a,b}); });
    // 只同步帧曲线视口，不重复解析数据。
    connect(timeline_, &TimelineWidget::viewportChanged, frameChart, [frameChart](qint64 a,qint64 b) { frameChart->setRange({a,b}); });
    connect(frameChart, &FrameTimeWidget::framePicked, timeline_, &TimelineWidget::focusEvent);
    plotsLayout->addWidget(timeline_,2,0);
    auto* scroll = new QScrollBar(Qt::Vertical); scroll->setObjectName("trackScrollBar");
    scroll->setRange(0,0); scroll->setEnabled(false); scroll->setToolTip(QStringLiteral("上下滚动轨道")); plotsLayout->addWidget(scroll,2,1); plotsLayout->setColumnStretch(0,1); plotsLayout->setRowStretch(2,1); layout->addLayout(plotsLayout,1);
    connect(scroll, &QScrollBar::valueChanged, timeline_, &TimelineWidget::setFirstTrack);
    // 阻断滚动条反馈信号后设置范围/页步长/位置，避免双向连接递归。
    connect(timeline_, &TimelineWidget::trackScrollChanged, scroll, [scroll](int first,int maximum,int page) {
        const QSignalBlocker blocker(scroll); scroll->setRange(0,maximum); scroll->setEnabled(maximum>0); scroll->setPageStep(page); scroll->setValue(first);
    });
    setCentralWidget(central);
    auto* detailDock = new QDockWidget(QStringLiteral("事件 / 分析 / 学习"),this); detailDock->setObjectName("detailDock");
    auto* tabs = new QTabWidget; tabs->setObjectName("detailTabs"); tabs->setMinimumWidth(310);
    auto* detail = new QTextBrowser; detail->setPlainText(QStringLiteral("点击事件查看详情；金色边框标明当前选中事件。\n\n框选时间范围后查看统计。"));
    tabs->addTab(detail,QStringLiteral("事件"));
    auto* analysisPage = new QWidget; auto* analysisLayout = new QVBoxLayout(analysisPage);
    auto* group = new QComboBox; group->setObjectName("statisticsGroup"); analysisLayout->addWidget(group);
    auto* summary = new QTextBrowser; summary->setObjectName("statisticsSummary"); summary->setMinimumHeight(160); analysisLayout->addWidget(summary,1);
    auto* longest = new QPushButton(QStringLiteral("定位最长事件 / 帧")); longest->setEnabled(false); analysisLayout->addWidget(longest);
    auto* longTable = new QTableWidget(0,2); longTable->setObjectName("longFrames"); longTable->setHorizontalHeaderLabels({QStringLiteral("长帧时刻 ms"),QStringLiteral("帧间隔 ms")});
    longTable->setEditTriggers(QAbstractItemView::NoEditTriggers); longTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    longTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); analysisLayout->addWidget(longTable,1);
    tabs->addTab(analysisPage,QStringLiteral("统计"));
    auto* learning = new QTextBrowser;
    learning->setPlainText(QStringLiteral("操作\n轨道名称区滚轮：上下滚动\n时间轴滚轮：鼠标锚点缩放\n中键：平移；左键：框选/拾取\nHome：全览；Esc：清除选择\n\n左侧勾选/过滤/折叠轨道。统计只包含当前显示轨道；真实帧按单个进程/交换链分析。\n\n面试路径\nQ01/03：索引、裁剪、LOD\nQ04/05/06：邮箱、快照、取消与代次\nQ08：缓存键与失效\nQ10：Release基准和原始测量\n\n来源边界\n教学CPU/GPU并非真实硬件事件。PresentMon帧间隔不是Kernel执行时长，也不是屏幕实际FPS。\n\n密集区显示概览桶；选中和统计仍使用原始事件。"));
    tabs->addTab(learning,QStringLiteral("学习"));
    auto* sourceInfo = new QTextBrowser; tabs->addTab(sourceInfo,QStringLiteral("来源")); detailDock->setWidget(tabs); addDockWidget(Qt::RightDockWidgetArea,detailDock);
    auto* tracksDock = new QDockWidget(QStringLiteral("轨道导航"),this); tracksDock->setObjectName("tracksDock");
    auto* tracksPage = new QWidget; auto* tracksLayout = new QVBoxLayout(tracksPage);
    auto* filter = new QLineEdit; filter->setPlaceholderText(QStringLiteral("按轨道名称过滤")); filter->setObjectName("trackFilter"); tracksLayout->addWidget(filter);
    auto* tree = new QTreeWidget; tree->setObjectName("trackTree"); tree->setHeaderHidden(true); tree->setMinimumWidth(175); tracksLayout->addWidget(tree);
    tracksDock->setWidget(tracksPage); addDockWidget(Qt::LeftDockWidgetArea,tracksDock);
    auto* framesDock = new QDockWidget(QStringLiteral("帧明细 / 分析导出"),this); framesDock->setObjectName("framesDock");
    auto* framesPanel = new FrameDetailsPanel; framesDock->setWidget(framesPanel); addDockWidget(Qt::BottomDockWidgetArea,framesDock); framesDock->hide();
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    viewMenu->addAction(tracksDock->toggleViewAction()); viewMenu->addAction(detailDock->toggleViewAction()); viewMenu->addAction(framesDock->toggleViewAction());
    auto range = std::make_shared<std::optional<TimeRange>>();
    auto sortState = std::make_shared<std::pair<FrameSort,bool>>(FrameSort::Start,false);
    // 统一汇总当前快照、轨道、组选项、范围和排序；先清空旧范围显示，再发后台统计请求。
    auto requestStats = [this,range,group,summary,longest,longTable,frameChart,heatmap,framesPanel,sortState] {
        statistics_.cancel(); longest->setEnabled(false); longTable->setRowCount(0);
        framesPanel->setAnalysis({}); heatmap->setAnalysis({});
        const auto snapshot = controller_.snapshot(); if (!snapshot) return;
        auto tracks = timeline_->tracks();
        if (snapshot->frames) {
            const auto id = group->currentData().toUInt();
            tracks = std::find(tracks.begin(),tracks.end(),id) == tracks.end() ? std::vector<std::uint32_t>{} : std::vector<std::uint32_t>{id};
        }
        frameChart->setVisible(snapshot->frames && !tracks.empty()); heatmap->setVisible(snapshot->frames && !tracks.empty());
        if(snapshot->frames && !tracks.empty()) { frameChart->setData(snapshot, tracks.front()); frameChart->setRange(timeline_->visibleRange()); }
        summary->setPlainText(QStringLiteral("正在后台计算原始数据统计…"));
        statistics_.request(snapshot, range->value_or(snapshot->bounds), std::move(tracks),sortState->first,sortState->second);
    };
    // 名称过滤、勾选和折叠共同产生源轨道ID列表；清除旧选区并重算当前显示数据统计。
    auto filterTracks = [this,tree,filter,range,requestStats] {
        std::vector<std::uint32_t> visible;
        for (int g=0; g<tree->topLevelItemCount(); ++g) {
            auto* parent = tree->topLevelItem(g);
            for (int i=0; i<parent->childCount(); ++i) {
                auto* item = parent->child(i);
                const bool matches = item->text(0).contains(filter->text(),Qt::CaseInsensitive);
                item->setHidden(!matches);
                if (matches && parent->isExpanded() && item->checkState(0)==Qt::Checked) visible.push_back(item->data(0,Qt::UserRole).toUInt());
            }
        }
        range->reset(); timeline_->setTracks(std::move(visible)); requestStats();
    };
    // 输入变化重新应用统一过滤逻辑，不在回调中复制筛选规则。
    connect(filter,&QLineEdit::textChanged,this,[filterTracks] { filterTracks(); });
    // 勾选变化刷新源轨道映射与统计。
    connect(tree,&QTreeWidget::itemChanged,this,[filterTracks] { filterTracks(); });
    // 折叠组后移除其可见轨道，同时更新统计口径。
    connect(tree,&QTreeWidget::itemCollapsed,this,[filterTracks] { filterTracks(); });
    // 展开组后恢复符合过滤和勾选条件的轨道。
    connect(tree,&QTreeWidget::itemExpanded,this,[filterTracks] { filterTracks(); });
    // 帧组切换发起新的单组分析，避免不同交换链混算。
    connect(group,&QComboBox::currentIndexChanged,this,[requestStats] { requestStats(); });
    // 清除范围状态并回到全会话统计，详情页提示没有事件选择。
    connect(timeline_,&TimelineWidget::selectionCleared,this,[range,detail,requestStats] {
        range->reset(); requestStats();
        detail->setPlainText(QStringLiteral("当前没有选中事件。点击事件查看详情，或框选后查看统计。"));
    });
    // 保存表头排序条件并重发后台分析，GUI不执行百万行排序。
    connect(framesPanel,&FrameDetailsPanel::sortRequested,this,[sortState,requestStats](int column,bool descending) {
        sortState->first=static_cast<FrameSort>(column); sortState->second=descending; requestStats();
    });
    connect(framesPanel,&FrameDetailsPanel::eventSelected,timeline_,&TimelineWidget::selectEvent);
    connect(framesPanel,&FrameDetailsPanel::eventActivated,timeline_,&TimelineWidget::focusEvent);
    // 把时间轴稳定ID交给表格恢复选择；表格内部阻断反向信号。
    connect(timeline_,&TimelineWidget::eventPicked,framesPanel,[framesPanel](qulonglong id,const QString&) { if(id) framesPanel->selectId(id); });
    connect(framesPanel,&FrameDetailsPanel::cancelExport,&exports_,&ExportController::cancel);
    connect(&exports_,&ExportController::progressChanged,framesPanel,&FrameDetailsPanel::setExportProgress);
    connect(&exports_,&ExportController::busyChanged,framesPanel,&FrameDetailsPanel::setExportBusy);
    // 在保存对话框前取得当前分析，之后始终导出该快照；控制器保护当前输入文件。
    connect(framesPanel,&FrameDetailsPanel::exportRequested,this,[this,framesPanel](ExportFormat format,const QString& notes) {
        const auto analysis=framesPanel->analysis(); if(!analysis) return;
        const bool markdown=format==ExportFormat::Markdown;
        const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("导出当前帧分析（仅本地）"),markdown?"analysis.md":"analysis.csv",markdown?"Markdown (*.md)":"CSV (*.csv)");
        if(!path.isEmpty()) exports_.request(path,analysis,format,notes,controller_.sourcePath());
    });
    // 显示原始事件详情并切换事件页，不改变统计范围。
    connect(timeline_,&TimelineWidget::eventPicked,this,[detail,tabs](qulonglong,const QString& text) { detail->setPlainText(text); tabs->setCurrentIndex(0); });
    // 保存半开纳秒选区并发后台统计，切到统计页展示结果。
    connect(timeline_,&TimelineWidget::rangeSelected,this,[range,requestStats,tabs](qint64 a,qint64 b) { *range=TimeRange{a,b}; requestStats(); tabs->setCurrentIndex(1); });
    connect(&statistics_,&StatisticsController::failed,summary,&QTextBrowser::setPlainText);
    // 同一结果同时绑定表格、热力图及统计页；按稳定ID恢复选择，避免视图各自使用不同代次。
    connect(&statistics_,&StatisticsController::ready,this,[this,range,summary,longest,longTable,framesPanel,heatmap] {
        framesPanel->setAnalysis(statistics_.frameDetails()); heatmap->setAnalysis(statistics_.frameDetails());
        if(const auto selected=timeline_->selectedEvent()) framesPanel->selectId(selected->id);
        const auto& stats=statistics_.result(); const bool frames=controller_.snapshot()->frames;
        QString text = range->has_value() ? QStringLiteral("选区 [%1, %2) ms\n").arg(double((*range)->begin)/1e6).arg(double((*range)->end)/1e6) : QStringLiteral("全会话 / 当前显示轨道\n");
        text += frames ? QStringLiteral("单进程/交换链；按Present时间归属，完整帧间隔\n") : QStringLiteral("相交事件；时长裁剪到选区，并发求和可超过墙钟时间\n");
        // 无样本显示N/A而非误导性的0，非空值按毫秒保留三位小数。
        auto metric = [&stats](double value) { return stats.count ? QString::number(value,'f',3) : QStringLiteral("N/A"); };
        text += QStringLiteral("数量 %1\n总时长 %2 ms\n均值 %3 ms\nP50 / P95 / P99：%4 / %5 / %6 ms\n").arg(stats.count).arg(stats.sumMs,0,'f',3).arg(metric(stats.meanMs)).arg(metric(stats.p50Ms)).arg(metric(stats.p95Ms)).arg(metric(stats.p99Ms));
        if (frames && stats.count) text += QStringLiteral("间隔口径FPS %1\n长帧 %2（60FPS预算，历史中位数规则v1）\n").arg(1000/stats.meanMs,0,'f',2).arg(stats.longFrames);
        if (!stats.count) text += QStringLiteral("无有效样本；百分位和FPS不可用。\n");
        text += QStringLiteral("\n时长分布（ms / 数量）\n≤8.33: %1\n(8.33,16.67]: %2\n(16.67,33.33]: %3\n(33.33,50]: %4\n>50: %5\n").arg(stats.histogram[0]).arg(stats.histogram[1]).arg(stats.histogram[2]).arg(stats.histogram[3]).arg(stats.histogram[4]);
        if (frames) text += QStringLiteral("\n下表展示前200个长帧，双击定位；长帧计数不截断。");
        summary->setPlainText(text); longest->setEnabled(bool(stats.longest));
        longTable->setRowCount(int(stats.longEvents.size()));
        for (int i=0; i<int(stats.longEvents.size()); ++i) {
            const auto& e=stats.longEvents[std::size_t(i)];
            longTable->setItem(i,0,new QTableWidgetItem(QString::number(double(e.start)/1e6,'f',3)));
            longTable->setItem(i,1,new QTableWidgetItem(QString::number(double(e.duration)/1e6,'f',3)));
        }
    });
    // 从已发布统计取最长事件并定位，空结果不执行操作。
    connect(longest,&QPushButton::clicked,this,[this] { if (statistics_.result().longest) timeline_->focusEvent(*statistics_.result().longest); });
    // 快捷列表行只索引当前longEvents，检查边界后再定位原始事件。
    connect(longTable,&QTableWidget::cellDoubleClicked,this,[this](int row,int) { const auto& events=statistics_.result().longEvents; if(row>=0 && std::size_t(row)<events.size()) timeline_->focusEvent(events[std::size_t(row)]); });
    auto* diagnostics=new QLabel; statusBar()->addPermanentWidget(diagnostics,1);
    connect(timeline_,&TimelineWidget::diagnosticsChanged,diagnostics,&QLabel::setText);
    // 收到确定进度后将进度条从忙指示切为百分比模式。
    connect(&controller_,&SessionController::progressChanged,progress,[progress](int value) { if(value>0) { progress->setRange(0,100); progress->setValue(value); } });
    auto* state = new QLabel(QStringLiteral("空会话")); statusBar()->addWidget(state);
    // 在状态栏显示导出错误/取消，所有控件操作留在GUI线程。
    connect(&exports_,&ExportController::message,this,[state](const QString& text) { state->setText(text); });
    // 成功提交后只显示报告文件名，避免把本机完整路径放进普通状态文本。
    connect(&exports_,&ExportController::completed,this,[state](const QString& path) { state->setText(QStringLiteral("已导出发起时的分析：%1").arg(QFileInfo(path).fileName())); });
    // 把加载失败/取消等状态转为界面提示，不清空旧成功快照。
    connect(&controller_,&SessionController::message,this,[state](const QString& text) { state->setText(text); });
    // 同步取消按钮和进度条忙闲；保留已有错误提示，不用通用“就绪”覆盖。
    connect(&controller_,&SessionController::busyChanged,this,[cancel,progress,state](bool busy) {
        cancel->setEnabled(busy); if(busy) { state->setText(QStringLiteral("后台加载中 · 可取消")); progress->setRange(0,0); }
        else { progress->setRange(0,100); progress->setValue(0); if(state->text().startsWith(QStringLiteral("后台"))) state->setText(QStringLiteral("就绪")); }
    });
    // 新快照发布后统一重建UI数据绑定；重建导航时阻断信号，再请求一份当前统计。
    connect(&controller_,&SessionController::snapshotReady,this,[this,heading,legend,tree,group,filter,range,detail,sourceInfo,state,requestStats,framesPanel,framesDock] {
        const auto snapshot=controller_.snapshot(); statistics_.cancel(); range->reset(); framesPanel->resetSession(); framesDock->setVisible(snapshot->frames); timeline_->setSnapshot(snapshot);
        heading->setText(QStringLiteral("%1 · %2 条记录 · %3 轨道 · 加载 %4 ms").arg(snapshot->synthetic?QStringLiteral("教学模拟 seed 42"):QStringLiteral("外部PresentMon帧CSV（来源真实性由采集记录确认）")).arg(snapshot->eventCount).arg(snapshot->tracks.size()).arg(controller_.loadMs(),0,'f',1));
        legend->setText(snapshot->frames ? QStringLiteral("帧矩形宽度：前一Present间隔（非GPU执行时长） | 金框：选中 | 长帧可在统计页定位") : QStringLiteral("<span style=\"color:#67a6e8\">■ CPU</span> <span style=\"color:#3cd7b1\">■ GPU</span> 金框：选中 | 概览桶计数 <span style=\"color:#20554f\">■ 1</span> <span style=\"color:#257562\">■ 4</span> <span style=\"color:#2f9d82\">■ 16</span> <span style=\"color:#39caa7\">■ 64+</span>（对数亮度，非利用率）"));
        const QSignalBlocker bt(tree), bg(group), bf(filter); tree->clear(); group->clear(); filter->clear();
        QMap<QString,QTreeWidgetItem*> groups;
        for(std::uint32_t i=0;i<snapshot->tracks.size();++i) {
            const auto name=QString::fromStdString(snapshot->tracks[i].name);
            const auto category=snapshot->frames?QStringLiteral("应用帧"):name.startsWith("CPU")?QStringLiteral("CPU"):QStringLiteral("GPU");
            if(!groups.contains(category)) { auto* parent=new QTreeWidgetItem(tree,{category}); parent->setFlags(parent->flags()|Qt::ItemIsAutoTristate|Qt::ItemIsUserCheckable); parent->setCheckState(0,Qt::Checked); parent->setExpanded(true); groups[category]=parent; }
            auto* item=new QTreeWidgetItem(groups[category],{name}); item->setData(0,Qt::UserRole,i); item->setCheckState(0,Qt::Checked); item->setToolTip(0,name);
            if(snapshot->frames) group->addItem(name,i);
        }
        if(!snapshot->frames) group->addItem(QStringLiteral("当前显示的教学轨道"));
        QString source=QString::fromStdString(snapshot->source)+"\nSHA-256: "+QString::fromStdString(snapshot->input.sha256)+"\n";
        for(const auto& warning:snapshot->warnings) source+=QString::fromStdString(warning)+"\n";
        sourceInfo->setPlainText(source); detail->setPlainText(source+QStringLiteral("\n点击事件查看原始记录。")); state->setText(QStringLiteral("加载完成")); requestStats();
    });
}
}
