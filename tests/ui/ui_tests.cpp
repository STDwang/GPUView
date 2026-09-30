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
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTableView>
#include <QAbstractItemModelTester>
#include <QAction>
#include <QCheckBox>
#include <QProgressBar>
#include <QLabel>
using namespace gpuview;
/// Qt Test界面回归集合；在离屏环境模拟输入并断言可观察状态。
class UiTests : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(model->eventAt(0)->duration,TimeNs(92316100)); table->setCurrentIndex(model->index(0,0));
        QVERIFY(window.timeline()->selectedEvent()); QCOMPARE(window.timeline()->selectedEvent()->id,model->eventAt(0)->id);
        auto* heat=window.findChild<FrameHeatmap*>(); QCOMPARE(heat->width(),window.timeline()->width());
        QTest::mouseClick(heat,Qt::LeftButton,Qt::NoModifier,QPoint(155,35));
        QTRY_VERIFY_WITH_TIMEOUT(model->analysis() && model->analysis()->range.end==1000000000,5000);
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
