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
#include <QTableView>
#include <QAbstractItemModelTester>
using namespace gpuview;
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void frameModelContractAndStableSelection() {
        FrameDetailsPanel panel; auto* model=panel.findChild<FrameTableModel*>(); QAbstractItemModelTester tester(model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto source=buildStore({{9,0,2000000,0,0},{3,10000000,10000000,0,0}},{"app"},{"frame"},1,{}, {},true,true);
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0})); panel.selectId(9);
        panel.setAnalysis(analyzeFrames(source,source->bounds,{0},FrameSort::Duration,true));
        auto* table=panel.findChild<QTableView*>(); QCOMPARE(table->currentIndex().data(Qt::UserRole).toULongLong(),qulonglong(9));
        QCOMPARE(model->eventAt(0)->id,std::uint64_t(3)); QCOMPARE(model->rowCount(model->index(0,0)),0);
    }
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
    void rendersBoundedGeometry() {
        TimelineWidget widget;
        widget.resize(1000, 600); widget.setSnapshot(generateTrace(100000, 1)); widget.show();
        QVERIFY(!widget.grab().isNull());
        QVERIFY(widget.primitiveCount() > 0);
        QVERIFY(widget.primitiveCount() < 35000);
        widget.setCacheEnabled(false); const auto uncached=widget.grab().toImage();
        widget.setCacheEnabled(true); QCOMPARE(widget.grab().toImage(),uncached);
    }
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
    void closeWhileBuilding() {
        auto window = std::make_unique<MainWindow>(); window->show();
        window->controller()->requestSynthetic(1000000);
        window.reset(); // 有活动worker时销毁窗口，不能触发QThread destroyed while running。
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
