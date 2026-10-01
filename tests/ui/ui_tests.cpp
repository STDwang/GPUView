/// @file tests/ui/ui_tests.cpp
/// @brief 离屏Widgets交互回归：模型契约、稳定ID、图表联动、滚动和退出；不测真实屏幕FPS。
#include "ui_widgets/timeline_widget.h"
#include "ui_widgets/main_window.h"
#include "adapters/synthetic_source.h"
#include <QtTest>
#include <QScrollBar>
#include <QDockWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QTableWidget>
#include <QTabWidget>
#include "ui_widgets/frame_time_widget.h"
#include "ui_widgets/frame_details_panel.h"
#include "ui_widgets/frame_heatmap.h"
#include "ui_widgets/event_explorer.h"
#include "ui_widgets/theme.h"
#include "ui_widgets/time_axis.h"
#include "ui_widgets/statistics_view.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTableView>
#include <QAbstractItemModelTester>
#include <QAction>
#include <QCheckBox>
#include <QProgressBar>
#include <QLabel>
#include <QHeaderView>
using namespace gpuview;
/// Qt Test界面回归集合；在离屏环境模拟输入并断言可观察状态。
class UiTests : public QObject {
    Q_OBJECT
private slots:
    /// 使用实际应用的中文字体和样式，避免默认测试字体掩盖窗口最小尺寸问题。
    void initTestCase() { configureApplicationTheme(*qApp); }
    /// 同列悬停和名称区移动不反复绘制时间轴，横向移动及离开仍更新十字线。
    void hoverRepaintsOnlyWhenCrosshairChanges() {
        TimelineWidget timeline; timeline.resize(1000,360); timeline.setSnapshot(generateTrace(10000,1)); timeline.show();
        QCoreApplication::processEvents(); QSignalSpy painted(&timeline,&TimelineWidget::diagnosticsChanged);
        // 直接发送逻辑坐标事件，不依赖系统光标的位置或窗口焦点。
        const auto move=[&](QPointF point) {
            QMouseEvent event(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(&timeline,&event); QCoreApplication::processEvents();
        };
        move({300,70}); QVERIFY(painted.count()>0); painted.clear();
        move({300,75}); move({300,80}); QCOMPARE(painted.count(),0);
        move({310,80}); QVERIFY(painted.count()>0); painted.clear();
        move({100,80}); QVERIFY(painted.count()>0); painted.clear();
        move({110,80}); move({120,80}); QCOMPARE(painted.count(),0);
        move({300,80}); painted.clear();
        QEvent leave(QEvent::Leave); QApplication::sendEvent(&timeline,&leave); QCoreApplication::processEvents();
        QVERIFY(painted.count()>0); painted.clear();
        QApplication::sendEvent(&timeline,&leave); QCoreApplication::processEvents(); QCOMPARE(painted.count(),0);
    }
    /// 拖动由发起按钮拥有；无关按钮不能中断，过滤/全览后的迟到释放不能恢复旧选区。
    void timelineGesturesRespectButtonsAndContext() {
        TimelineWidget timeline; timeline.resize(1000,360); timeline.show();
        const auto source=generateTrace(10000,1); timeline.setSnapshot(source);
        QSignalSpy selected(&timeline,&TimelineWidget::rangeSelected);
        QSignalSpy cleared(&timeline,&TimelineWidget::selectionCleared);
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(300,70));
        QTest::mouseMove(&timeline,QPoint(500,70));
        QTest::mouseClick(&timeline,Qt::RightButton,Qt::NoModifier,QPoint(500,70)); QCOMPARE(selected.count(),0);
        QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(500,70)); QCOMPARE(selected.count(),1);
        const auto clearCount=cleared.count();
        QTest::mouseClick(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(995,70)); QCOMPARE(cleared.count(),clearCount);
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(300,70)); QTest::mouseMove(&timeline,QPoint(500,70));
        timeline.setTracks({0}); QTest::mouseMove(&timeline,QPoint(600,70));
        QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(600,70)); QCOMPARE(selected.count(),1);
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(300,55)); QTest::mouseMove(&timeline,QPoint(500,55));
        timeline.resetViewport(); QTest::mouseMove(&timeline,QPoint(600,55));
        QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(600,55)); QCOMPARE(selected.count(),1);
        const auto full=timeline.visibleRange(); timeline.showRange({full.end/4,full.end/2});
        QTest::mousePress(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(500,55)); QTest::mouseMove(&timeline,QPoint(480,55));
        const auto beforeUnrelatedRelease=timeline.visibleRange();
        QTest::mouseRelease(&timeline,Qt::RightButton,Qt::NoModifier,QPoint(480,55)); QTest::mouseMove(&timeline,QPoint(460,55));
        QVERIFY(!(timeline.visibleRange()==beforeUnrelatedRelease));
        QTest::mouseRelease(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(460,55));
        timeline.setTracks({}); const auto emptyClearCount=cleared.count();
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(300,70)); QTest::mouseMove(&timeline,QPoint(600,70));
        QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(600,70));
        QCOMPARE(selected.count(),1); QCOMPARE(cleared.count(),emptyClearCount);
    }
    /// 帧明细在较窄面板和导出忙状态仍容纳操作，行高要留出实际字体及上下内边距。
    void frameDetailsRemainReadableAtCompactWidth() {
        FrameDetailsPanel panel; panel.setStyleSheet(darkTheme()); panel.resize(760,300); panel.show();
        const auto source=buildStore({{1,0,1000000,0,0},{2,2000000,2000000,0,0}},{"frames"},{"Present"},1,{}, {},false,true);
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0})); QCoreApplication::processEvents();
        QCOMPARE(panel.width(),760);
        auto* notes=panel.findChild<QLineEdit*>("exportNotes"); QVERIFY(notes); QVERIFY(notes->width()>=240);
        notes->setText(QStringLiteral("检查帧间隔尖峰"));
        auto* cancel=panel.findChild<QPushButton*>("cancelFrameExport"); QVERIFY(cancel); QVERIFY(cancel->isHidden());
        auto* table=panel.findChild<QTableView*>("frameDetailsTable");
        QVERIFY(table->verticalHeader()->defaultSectionSize()>=table->fontMetrics().height()+6);
        auto* exportButton=panel.findChild<QPushButton*>("exportAnalysis"); QVERIFY(exportButton->isEnabled());
        panel.setExportBusy(true); panel.setExportProgress(100); QCoreApplication::processEvents();
        QCOMPARE(panel.width(),760); QVERIFY(!exportButton->isEnabled()); QVERIFY(cancel->isVisible()); QVERIFY(cancel->isEnabled());
        QVERIFY(notes->width()>=240); QVERIFY(notes->geometry().right()<exportButton->geometry().left());
        QSignalSpy cancelled(&panel,&FrameDetailsPanel::cancelExport); QTest::mouseClick(cancel,Qt::LeftButton); QCOMPARE(cancelled.count(),1);
        panel.setAnalysis({}); panel.setExportBusy(false); QVERIFY(!exportButton->isEnabled()); QVERIFY(cancel->isHidden());
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0})); QVERIFY(exportButton->isEnabled());
        QCOMPARE(notes->text(),QStringLiteral("检查帧间隔尖峰"));
        QCoreApplication::processEvents();
        const auto capture=qEnvironmentVariable("GPUVIEW_FRAME_PANEL_CAPTURE");
        if(!capture.isEmpty()) QVERIFY(panel.grab().save(capture));
    }
    /// 帧图仅接受绘图区左击，最近帧必须属于半开可见范围；热力图留白不映射末桶。
    void chartPickingRespectsPlotAndVisibleSamples() {
        auto source=buildStore({{1,10,5,0,0},{2,50,5,0,0},{3,90,5,0,0}},{"frame"},{"Present"},1,{}, {},false,true);
        FrameTimeWidget chart; chart.resize(600,150); chart.setData(source,0); chart.show();
        std::uint64_t id=0;
        // 记录源ID而不是绘制像素，验证命中返回的是可见原始记录。
        connect(&chart,&FrameTimeWidget::framePicked,&chart,[&id](const Event& event) { id=event.id; });
        chart.setRange({40,60});
        QTest::mouseClick(&chart,Qt::RightButton,Qt::NoModifier,QPoint(300,70)); QCOMPARE(id,std::uint64_t(0));
        QTest::mouseClick(&chart,Qt::LeftButton,Qt::NoModifier,QPoint(300,15)); QCOMPARE(id,std::uint64_t(0));
        QTest::mouseClick(&chart,Qt::LeftButton,Qt::NoModifier,QPoint(590,70)); QCOMPARE(id,std::uint64_t(0));
        QTest::mouseClick(&chart,Qt::LeftButton,Qt::NoModifier,QPoint(155,70)); QCOMPARE(id,std::uint64_t(2));
        id=0; chart.setRange({20,50}); // 终点50处的帧也不属于当前视口。
        QTest::mouseClick(&chart,Qt::LeftButton,Qt::NoModifier,QPoint(580,70)); QCOMPARE(id,std::uint64_t(0));
        FrameHeatmap heat; heat.resize(600,78); heat.setAnalysis(analyzeFrames(source,source->bounds,{0})); heat.show();
        QSignalSpy picked(&heat,&FrameHeatmap::rangePicked);
        QTest::mouseClick(&heat,Qt::LeftButton,Qt::NoModifier,QPoint(590,35));
        QTest::mouseClick(&heat,Qt::LeftButton,Qt::NoModifier,QPoint(300,49));
        QTest::mouseClick(&heat,Qt::LeftButton,Qt::NoModifier,QPoint(300,18)); QCOMPARE(picked.count(),0);
        QTest::mouseClick(&heat,Qt::LeftButton,Qt::NoModifier,QPoint(150,27)); QCOMPARE(picked.count(),1);
    }
    /// 导航动作只反映实际可执行操作；全览边界、重复范围和中键点击不制造虚假返回历史。
    void navigationActionsFollowEffectiveChanges() {
        MainWindow window; window.show();
        auto* overview=window.findChild<QAction*>("overviewAction");
        auto* back=window.findChild<QAction*>("previousViewAction");
        auto* zoom=window.findChild<QAction*>("zoomSelectionAction");
        QVERIFY(!overview->isEnabled()); QVERIFY(!back->isEnabled()); QVERIFY(!zoom->isEnabled());
        window.controller()->requestSynthetic(10000); QTRY_VERIFY_WITH_TIMEOUT(overview->isEnabled(),5000);
        auto* timeline=window.timeline(); const auto full=timeline->visibleRange();
        timeline->showRange(full); timeline->resetViewport(); timeline->resetViewport();
        QTest::mouseClick(timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(400,70));
        QWheelEvent outward(QPointF(400,70),QPointF(400,70),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(timeline,&outward); QVERIFY(!back->isEnabled());
        const TimeRange selected{full.end/4,full.end/2}; timeline->selectRange(selected);
        QVERIFY(back->isEnabled()); QVERIFY(!zoom->isEnabled());
        back->trigger(); QVERIFY(timeline->visibleRange()==full); QVERIFY(!back->isEnabled()); QVERIFY(zoom->isEnabled());
        zoom->trigger(); timeline->zoomSelection(); back->trigger();
        QVERIFY(timeline->visibleRange()==full); QVERIFY(!back->isEnabled());
        QTest::keyClick(timeline,Qt::Key_Escape); QVERIFY(!zoom->isEnabled());
    }
    /// 多次移动的一次平移只占一条历史，拖回原点和换数据后的旧鼠标释放不污染导航。
    void panGestureHistoryAndSnapshotReset() {
        TimelineWidget timeline; timeline.resize(1000,360); timeline.show();
        auto source=generateTrace(10000,1); timeline.setSnapshot(source);
        const auto full=timeline.visibleRange(); timeline.showRange({full.end/4,full.end/2});
        const auto before=timeline.visibleRange();
        QTest::mousePress(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(500,70));
        QTest::mouseMove(&timeline,QPoint(480,70)); QTest::mouseMove(&timeline,QPoint(460,70));
        QTest::mouseRelease(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(460,70));
        QVERIFY(!(timeline.visibleRange()==before)); timeline.previousView(); QVERIFY(timeline.visibleRange()==before);
        QTest::mousePress(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(500,70));
        QTest::mouseMove(&timeline,QPoint(480,70)); QTest::mouseMove(&timeline,QPoint(500,70));
        QTest::keyClick(&timeline,Qt::Key_Escape);
        QTest::mouseRelease(&timeline,Qt::MiddleButton,Qt::NoModifier,QPoint(500,70));
        timeline.previousView(); QVERIFY(timeline.visibleRange()==full);
        QSignalSpy selected(&timeline,&TimelineWidget::rangeSelected);
        QSignalSpy navigation(&timeline,&TimelineWidget::navigationChanged);
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(400,70));
        timeline.setSnapshot(generateTrace(10000,2));
        QTest::mouseMove(&timeline,QPoint(600,70)); QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(600,70));
        QCOMPARE(selected.count(),0);
        for(const auto& state:navigation) QVERIFY(!state.at(1).toBool());
    }
    /// 较矮统计区域无需滚动即可看到P99；空结果不能保留上次的有效值，Trace不能冒充FPS。
    void compactStatisticsPrioritizeMetricsAndClearEmptyValues() {
        Statistics stats; stats.count=905; stats.meanMs=8.192; stats.p95Ms=7.735; stats.p99Ms=82.945;
        QTextBrowser browser; browser.setStyleSheet(darkTheme()); browser.resize(290,120);
        browser.setHtml(statisticsHtml(stats,true,std::nullopt)); browser.show();
        QCoreApplication::processEvents();
        const auto cursor=browser.document()->find("82.945"); QVERIFY(!cursor.isNull());
        QVERIFY2(browser.cursorRect(cursor).bottom()<=browser.viewport()->height(),"P99 must fit the initial viewport");
        browser.setHtml(statisticsHtml({},true,TimeRange{0,1}));
        QVERIFY(browser.toPlainText().contains(QStringLiteral("当前范围无有效样本")));
        QVERIFY(browser.toPlainText().contains("N/A")); QVERIFY(!browser.toPlainText().contains("82.945"));
        browser.setHtml(statisticsHtml(stats,false,std::nullopt));
        QVERIFY(browser.toPlainText().contains(QStringLiteral("并发求和"))); QVERIFY(!browser.toPlainText().contains("FPS"));
    }
    /// 不同范围、宽度、字体下标签均不越界/重叠；减少刻度不能改变首末时间位置。
    void timeAxisLabelsFitAvailableWidth() {
        for(int pointSize:{10,16}) {
            QFont font=qApp->font(); font.setPointSize(pointSize); const QFontMetrics metrics(font);
            for(int width:{40,160,318,400,1000}) for(TimeRange range:{TimeRange{0,7457961000LL},TimeRange{10000000,10000100},TimeRange{0,120000000000LL}}) {
                const auto ticks=timeAxisTicks(range,width,metrics); QVERIFY(!ticks.empty()); QVERIFY(ticks.size()<=9);
                QCOMPARE(ticks.front().x,0); int previousRight=-10;
                for(const auto& tick:ticks) {
                    QVERIFY(tick.labelLeft>=previousRight+10);
                    const int right=tick.labelLeft+metrics.horizontalAdvance(tick.text); QVERIFY(right<=width);
                    previousRight=right;
                }
                if(ticks.size()>1) QCOMPARE(ticks.back().x,width);
            }
        }
        QVERIFY(timeAxisTicks({1,1},400,QFontMetrics(qApp->font())).empty());
        QVERIFY(timeAxisTicks({0,1},0,QFontMetrics(qApp->font())).empty());
    }
    /// 真实帧模式应适配1200x800逻辑像素；可选环境路径保存高DPI截图用于目视复核。
    void frameWorkspaceFitsCompactWindow() {
        MainWindow window; window.resize(1200,800); window.show();
        window.controller()->requestFile(QFINDTESTDATA("../../data/samples/presentmon-real.csv"));
        auto* model=window.findChild<EventAnalysisModel*>("eventResultsModel");
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),905,5000);
        QTRY_COMPARE_WITH_TIMEOUT(window.findChild<FrameTableModel*>()->rowCount(),905,5000);
        QCOMPARE(window.size(),QSize(1200,800));
        auto* table=window.findChild<QTableView*>("eventResults");
        QVERIFY(table->viewport()->height()>=2*table->verticalHeader()->defaultSectionSize());
        QVERIFY(window.timeline()->height()>=120);
        QCOMPARE(window.findChild<FrameTimeWidget*>()->width(),window.timeline()->width());
        const auto capture=qEnvironmentVariable("GPUVIEW_TEST_CAPTURE");
        if(!capture.isEmpty()) QVERIFY(window.grab().save(capture));
    }
    /// 非法上下限不发布旧结果，回车纠正立即提交；快捷导航沿当前排序前后切换。
    void searchValidationAndKeyboardNavigation() {
        EventExplorer panel; panel.resize(1100,320); panel.show();
        const auto source=buildStore({{1,0,1000000,0,0},{2,2000000,2000000,0,0}},{"track"},{"work"},1);
        auto* model=panel.findChild<EventAnalysisModel*>("eventResultsModel");
        panel.setContext(source,{0},std::nullopt); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),2,5000);
        panel.findChild<QDoubleSpinBox*>("eventMinimum")->setValue(2);
        panel.findChild<QDoubleSpinBox*>("eventMaximum")->setValue(1);
        auto* status=panel.findChild<QLabel*>("eventStatus");
        QTRY_VERIFY(status->text().contains(QStringLiteral("上限不能小于下限")));
        QVERIFY(!model->result()); QVERIFY(!panel.findChild<QPushButton*>("eventCancel")->isEnabled());
        auto* search=panel.findChild<QLineEdit*>("eventSearch"); panel.focusSearch();
        panel.findChild<QDoubleSpinBox*>("eventMaximum")->setValue(0);
        QTest::keyClick(search,Qt::Key_Return); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),1,5000);
        QCOMPARE(model->eventAt(0)->id,std::uint64_t(2));
        QTest::mouseClick(panel.findChild<QPushButton*>("eventReset"),Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),2,5000); QSignalSpy activated(&panel,&EventExplorer::eventActivated);
        QTest::keyClick(search,Qt::Key_F3); QTRY_COMPARE(activated.count(),1);
        auto* table=panel.findChild<QTableView*>("eventResults"); QCOMPARE(table->currentIndex().row(),0);
        QTest::keyClick(search,Qt::Key_F3); QTRY_COMPARE(activated.count(),2); QCOMPARE(table->currentIndex().row(),1);
        QTest::keyClick(search,Qt::Key_F3,Qt::ShiftModifier); QTRY_COMPARE(activated.count(),3); QCOMPARE(table->currentIndex().row(),0);
    }
    /// 表格导航只在目标轨道离开视野时最小滚动，已可见行不跳到顶端。
    void selectedTrackScrollsOnlyWhenOutsideViewport() {
        TimelineWidget widget; widget.resize(1000,360); widget.show();
        const auto source=generateTrace(10000,1); widget.setSnapshot(source); widget.setFirstTrack(10);
        QSignalSpy scroll(&widget,&TimelineWidget::trackScrollChanged);
        widget.selectEvent(source->tracks[12].index.events().front()); QCOMPARE(scroll.count(),0);
        widget.selectEvent(source->tracks[24].index.events().front()); QCOMPARE(scroll.count(),1);
        QCOMPARE(scroll.last().at(0).toInt(),15);
        widget.selectEvent(source->tracks[3].index.events().front()); QCOMPARE(scroll.count(),2);
        QCOMPARE(scroll.last().at(0).toInt(),3);
    }
    /// 恢复布局不换会话、不清筛选；空闲加载进度隐藏，恢复的帧Dock遵守当前数据类型。
    void defaultLayoutPreservesDataAndFilters() {
        MainWindow window; window.show(); QVERIFY(!window.findChild<QProgressBar*>("loadProgress")->isVisible());
        window.controller()->requestSynthetic(10000);
        auto* model=window.findChild<EventAnalysisModel*>("eventResultsModel");
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),10000,5000);
        QVERIFY(!window.findChild<QProgressBar*>("loadProgress")->isVisible());
        const auto source=window.controller()->snapshot();
        window.findChild<QLineEdit*>("eventSearch")->setText("Kernel");
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),5000,5000);
        auto* tracks=window.findChild<QDockWidget*>("tracksDock");
        window.addDockWidget(Qt::RightDockWidgetArea,tracks); tracks->hide();
        window.findChild<QDockWidget*>("eventsDock")->hide();
        auto* action=window.findChild<QAction*>("resetLayoutAction"); QVERIFY(action); action->trigger();
        QCOMPARE(window.dockWidgetArea(tracks),Qt::LeftDockWidgetArea); QVERIFY(tracks->isVisible());
        QVERIFY(window.findChild<QDockWidget*>("eventsDock")->isVisible());
        QVERIFY(!window.findChild<QDockWidget*>("framesDock")->isVisible());
        QCOMPARE(window.controller()->snapshot(),source); QCOMPARE(model->rowCount(),5000);
        QCOMPARE(window.findChild<QLineEdit*>("eventSearch")->text(),QString("Kernel"));
    }
    /// 搜索模型契约、名称筛选、排序后ID、汇总和空结果导航均通过真实GUI输入验证。
    void eventExplorerSearchSortAndNavigation() {
        EventExplorer panel; panel.resize(1100,320); panel.show();
        auto* model=panel.findChild<EventAnalysisModel*>("eventResultsModel");
        auto* groups=panel.findChild<EventAnalysisModel*>("eventGroupsModel");
        QAbstractItemModelTester a(model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester b(groups,QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto source=buildStore({{9,0,100,0,0},{3,100,300,0,0},{5,600,50,0,1}},{"track"},{"task","other"},1);
        panel.setContext(source,{0},std::nullopt); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),3,5000);
        panel.selectId(9); model->sort(4,Qt::DescendingOrder);
        QTRY_VERIFY_WITH_TIMEOUT(model->result() && model->result()->filter.eventColumn==4,5000);
        QVERIFY(model->result()->reusedSelection);
        auto* table=panel.findChild<QTableView*>("eventResults"); QCOMPARE(model->eventAt(table->currentIndex().row())->id,std::uint64_t(9));
        panel.findChild<QLineEdit*>("eventSearch")->setText("task");
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),2,5000); QCOMPARE(groups->rowCount(),1);
        panel.setContext(source,{0},TimeRange{100,200}); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),1,5000);
        QCOMPARE(model->result()->rows[0].contribution,TimeNs(100));
        QSignalSpy selected(&panel,&EventExplorer::eventActivated);
        QTest::mouseClick(panel.findChild<QPushButton*>("eventNext"),Qt::LeftButton); QCOMPARE(selected.count(),1);
        panel.findChild<QLineEdit*>("eventSearch")->setText("absent");
        QTRY_VERIFY_WITH_TIMEOUT(model->result() && model->result()->filter.text=="absent",5000);
        QCOMPARE(model->rowCount(),0); QVERIFY(!panel.findChild<QPushButton*>("eventNext")->isEnabled());
        panel.findChild<QDoubleSpinBox*>("eventMinimum")->setValue(1);
        panel.findChild<QDoubleSpinBox*>("eventMaximum")->setValue(2);
        panel.findChild<QCheckBox*>("eventSelectionOnly")->setChecked(false);
        QTest::mouseClick(panel.findChild<QPushButton*>("eventReset"),Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(model->result() && model->result()->filter.text.empty(),5000);
        QCOMPARE(model->rowCount(),1); QCOMPARE(model->result()->filter.minimum,TimeNs(0));
        QVERIFY(!model->result()->filter.maximum); QCOMPARE(model->result()->filter.eventColumn,4);
        QCOMPARE(model->data(model->index(0,4),Qt::TextAlignmentRole).toInt(),int(Qt::AlignRight|Qt::AlignVCenter));
    }
    /// 主窗口把搜索结果ID映射回时间轴，框选范围也传递给独立搜索面板。
    void eventExplorerWindowLink() {
        MainWindow window; window.show(); window.controller()->requestSynthetic(100000);
        auto* explorer=window.findChild<EventExplorer*>(); auto* model=explorer->findChild<EventAnalysisModel*>("eventResultsModel");
        QTRY_VERIFY_WITH_TIMEOUT(model->rowCount()>0,5000);
        auto* dock=window.findChild<QDockWidget*>("eventsDock"); dock->hide();
        auto* search=window.findChild<QAction*>("searchEventsAction"); QVERIFY(search);
        QCOMPARE(search->shortcut(),QKeySequence(QKeySequence::Find)); search->trigger();
        QVERIFY(dock->isVisible());
        QTRY_VERIFY(explorer->findChild<QLineEdit*>("eventSearch")->hasFocus());
        const auto id=model->eventAt(0)->id;
        auto* table=explorer->findChild<QTableView*>("eventResults"); table->setCurrentIndex(model->index(0,0));
        QVERIFY(window.timeline()->selectedEvent()); QCOMPARE(window.timeline()->selectedEvent()->id,id);
        window.timeline()->selectRange({0,1000000});
        QTRY_VERIFY_WITH_TIMEOUT(model->result() && model->result()->filter.range.end==1000000,5000);
        window.controller()->requestSynthetic(1000000); // 析构同时覆盖加载和搜索Worker退出路径。
    }
    /// 用QAbstractItemModelTester检查模型契约，排序后按稳定ID恢复正确事件。
    void frameModelContractAndStableSelection() {
        FrameDetailsPanel panel; auto* model=panel.findChild<FrameTableModel*>(); QAbstractItemModelTester tester(model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto source=buildStore({{9,0,2000000,0,0},{3,10000000,10000000,0,0}},{"app"},{"frame"},1,{}, {},true,true);
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0})); panel.selectId(9);
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0},FrameSort::Duration,true));
        auto* table=panel.findChild<QTableView*>(); QCOMPARE(table->currentIndex().data(Qt::UserRole).toULongLong(),qulonglong(9));
        QCOMPARE(model->eventAt(0)->id,std::uint64_t(3)); QCOMPARE(model->rowCount(model->index(0,0)),0);
    }
    /// 验证真实帧排序、表格选中、热力图选区及Home恢复在三个视图间一致。
    void heatmapTableAndTimelineStayInSync() {
        MainWindow window; window.show(); const auto path=QFINDTESTDATA("../../data/samples/presentmon-real.csv"); window.controller()->requestFile(path);
        auto* model=window.findChild<FrameTableModel*>(); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),905,5000);
        auto* table=window.findChild<QTableView*>("frameDetailsTable"); table->sortByColumn(2,Qt::DescendingOrder);
        QTRY_VERIFY_WITH_TIMEOUT(model->analysis() && model->analysis()->sort==FrameSort::Duration,5000);
        QVERIFY(model->analysis()->reusedSelection);
        QCOMPARE(model->eventAt(0)->duration,TimeNs(92316100)); table->setCurrentIndex(model->index(0,0));
        QVERIFY(window.timeline()->selectedEvent()); QCOMPARE(window.timeline()->selectedEvent()->id,model->eventAt(0)->id);
        auto* heat=window.findChild<FrameHeatmap*>(); QCOMPARE(heat->width(),window.timeline()->width());
        QTest::mouseClick(heat,Qt::LeftButton,Qt::NoModifier,QPoint(155,35));
        QTRY_VERIFY_WITH_TIMEOUT(model->analysis() && model->analysis()->range.end==1000000000,5000);
        QVERIFY(!model->analysis()->reusedSelection);
        QVERIFY(model->rowCount()>0 && model->rowCount()<905);
        window.timeline()->resetViewport(); QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(),905,5000);
    }

    /// 导入真实夹具后验证统计、帧曲线宽度与长帧双击定位。
    void realFileAnalysisAndLongFrameNavigation() {
        MainWindow window; window.show();
        const auto path=QFINDTESTDATA("../../data/samples/presentmon-real.csv"); QVERIFY(!path.isEmpty());
        window.controller()->requestFile(path);
        auto* table=window.findChild<QTableWidget*>("longFrames");
        QTRY_VERIFY_WITH_TIMEOUT(table->rowCount()>0,5000);
        auto* chart=window.findChild<FrameTimeWidget*>(); QVERIFY(chart->isVisible());
        QCOMPARE(chart->width(),window.timeline()->width());
        QVERIFY(!chart->grab().isNull());
        const auto original=window.timeline()->visibleRange();
        QVERIFY(QMetaObject::invokeMethod(table,"cellDoubleClicked",Qt::DirectConnection,Q_ARG(int,0),Q_ARG(int,0)));
        QVERIFY(window.timeline()->selectedEvent().has_value());
        QVERIFY(window.timeline()->selectedEvent()->duration>33333333);
        QVERIFY(window.timeline()->visibleRange().end-window.timeline()->visibleRange().begin<original.end-original.begin);
        QCOMPARE(window.findChild<QTabWidget*>("detailTabs")->currentIndex(),0);
        // 清空可见轨道必须真正发布空分析，不能只在格式化函数中模拟空状态。
        auto* summary=window.findChild<QTextBrowser*>("statisticsSummary");
        auto* filter=window.findChild<QLineEdit*>("trackFilter"); filter->setText("no-matching-track");
        QTRY_VERIFY_WITH_TIMEOUT(summary->toPlainText().contains(QStringLiteral("当前范围无有效样本")),5000);
        QCOMPARE(table->rowCount(),0); QVERIFY(!window.findChild<QPushButton*>("locateLongest")->isEnabled());
        filter->clear(); QTRY_VERIFY_WITH_TIMEOUT(summary->toPlainText().contains("82.945"),5000);
        QVERIFY(window.findChild<QPushButton*>("locateLongest")->isEnabled());
    }

    /// 验证点击事件、详情选择、框选缩放与上一视图恢复。
    void eventSelectionAndZoomHistory() {
        TimelineWidget widget; widget.resize(1000,600);
        auto data=buildStore({{1,0,100,0,0},{2,200,100,1,0}},{"CPU / a","GPU / b"},{"work"},1);
        widget.setSnapshot(data); widget.show();
        QTest::mouseClick(&widget,Qt::LeftButton,Qt::NoModifier,QPoint(200,60));
        QVERIFY(widget.selectedEvent().has_value()); QCOMPARE(widget.selectedEvent()->id,std::uint64_t(1));
        const auto original=widget.visibleRange(); widget.focusEvent(data->tracks[0].index.events()[0]);
        QVERIFY(widget.visibleRange().end<original.end); widget.previousView(); QVERIFY(widget.visibleRange()==original);
        QTest::keyClick(&widget,Qt::Key_Escape); QVERIFY(!widget.selectedEvent());
        widget.setTracks({1}); QTest::mouseClick(&widget,Qt::LeftButton,Qt::NoModifier,QPoint(800,60));
        QVERIFY(widget.selectedEvent()); QCOMPARE(widget.selectedEvent()->track,std::uint32_t(1));
    }
    /// 验证轨道导航过滤、勾选及组折叠对时间轴/统计的影响。
    void navigationFilterAndGroupCollapse() {
        MainWindow window; window.show(); window.controller()->requestSynthetic(1000);
        QTRY_VERIFY_WITH_TIMEOUT(bool(window.controller()->snapshot()),5000);
        auto* filter=window.findChild<QLineEdit*>("trackFilter"); auto* tree=window.findChild<QTreeWidget*>("trackTree");
        filter->setText("CPU"); QCOMPARE(window.timeline()->tracks().size(),std::size_t(8));
        tree->topLevelItem(0)->setExpanded(false); QCOMPARE(window.timeline()->tracks().size(),std::size_t(0));
        tree->topLevelItem(0)->setExpanded(true); filter->clear(); QCOMPARE(window.timeline()->tracks().size(),std::size_t(64));
        auto* summary=window.findChild<QTextBrowser*>("statisticsSummary");
        QTRY_VERIFY_WITH_TIMEOUT(summary->toPlainText().contains(QStringLiteral("数量")),5000);
    }

    /// 验证滚动条位置、范围、页步长随轨道数和窗口高度正确更新。
    void trackScrollFollowsViewport() {
        MainWindow window;
        window.resize(1000, 600);
        window.show();
        auto* timeline = window.findChild<TimelineWidget*>();
        auto* scroll = window.findChild<QScrollBar*>("trackScrollBar");
        auto* dock = window.findChild<QDockWidget*>();
        QVERIFY(timeline && scroll && dock);
        QCOMPARE(scroll->orientation(), Qt::Vertical);
        QCOMPARE(scroll->maximum(), 0);
        timeline->setSnapshot(generateTrace(1000, 1));
        QCOMPARE(scroll->maximum() + scroll->pageStep(), 64);
        QVERIFY(scroll->mapToGlobal(QPoint()).x() >= timeline->mapToGlobal(QPoint(timeline->width(), 0)).x());
        QVERIFY(scroll->mapToGlobal(QPoint(scroll->width(), 0)).x() <= dock->mapToGlobal(QPoint()).x());
        scroll->setValue(scroll->maximum());
        const auto previousMaximum = scroll->maximum();
        window.resize(1000, 900);
        QTRY_VERIFY(scroll->maximum() < previousMaximum);
        QCOMPARE(scroll->value(), scroll->maximum());
        QCOMPARE(scroll->maximum() + scroll->pageStep(), 64);
        timeline->setSnapshot({});
        QCOMPARE(scroll->maximum(), 0);
        QCOMPARE(scroll->value(), 0);
    }
    /// 验证时间轴可离屏绘制且密集数据图元受限，并检查缓存开关图像一致。
    void rendersBoundedGeometry() {
        TimelineWidget widget;
        widget.resize(1000, 600); widget.setSnapshot(generateTrace(100000, 1)); widget.show();
        QVERIFY(!widget.grab().isNull());
        QVERIFY(widget.primitiveCount() > 0);
        QVERIFY(widget.primitiveCount() < 35000);
        widget.setCacheEnabled(false); const auto uncached=widget.grab().toImage();
        widget.setCacheEnabled(true); QCOMPARE(widget.grab().toImage(),uncached);
    }
    /// 模拟左键拖动，断言只发布有效半开纳秒选区。
    void selectionEmitsRange() {
        TimelineWidget widget;
        widget.resize(1000, 600); widget.setSnapshot(generateTrace(1000, 1)); widget.show();
        QSignalSpy spy(&widget, &TimelineWidget::rangeSelected);
        QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(200, 60));
        QTest::mouseMove(&widget, QPoint(500, 60));
        QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(500, 60));
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy[0][0].toLongLong() < spy[0][1].toLongLong());
    }
    /// 模拟鼠标滚轮缩放并用Home恢复全会话范围。
    void wheelAndHome() {
        TimelineWidget widget;
        widget.resize(1000, 600); widget.setSnapshot(generateTrace(1000, 1)); widget.show();
        const auto original = widget.visibleRange();
        QWheelEvent event(QPointF(500, 100), QPointF(500, 100), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&widget, &event);
        QVERIFY(widget.visibleRange().end - widget.visibleRange().begin < original.end - original.begin);
        QTest::keyClick(&widget, Qt::Key_Home);
        QVERIFY(widget.visibleRange() == original);
    }
    /// 窗口在后台加载中关闭时可安全析构，不遗留访问已销毁控件的任务。
    void closeWhileBuilding() {
        auto window = std::make_unique<MainWindow>(); window->show();
        window->controller()->requestSynthetic(1000000);
        window.reset(); // 有活动worker时销毁窗口，不能触发QThread destroyed while running。
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
