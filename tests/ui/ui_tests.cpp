#include "ui_widgets/timeline_widget.h"
#include "ui_widgets/main_window.h"
#include "adapters/synthetic_source.h"
#include <QtTest>
#include <QScrollBar>
#include <QDockWidget>
using namespace gpuview;
class UiTests : public QObject {
    Q_OBJECT
private slots:
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
