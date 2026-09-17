#include "core/clock_mapping.h"
#include "core/render_query.h"
#include "adapters/synthetic_source.h"
#include "application/session_controller.h"
#include <QtTest>
#include <limits>
#include <random>
using namespace gpuview;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void longIntervalCrossesViewport() {
        IntervalIndex index({{1, 0, 1000, 0, 0}, {2, 100, 5, 0, 0}, {3, 500, 10, 0, 0}});
        auto q = index.query({400, 450});
        QCOMPARE(q.events.size(), std::size_t(1));
        QCOMPARE(q.events.front()->id, std::uint64_t(1));
    }
    void halfOpenBoundaries() {
        IntervalIndex index({{1, 10, 10, 0, 0}, {2, 20, 10, 0, 0}});
        auto q = index.query({20, 21});
        QCOMPARE(q.events.size(), std::size_t(1));
        QCOMPARE(q.events.front()->id, std::uint64_t(2));
    }
    void emptyRange() { QVERIFY(IntervalIndex({{1, 0, 10, 0, 0}}).query({5, 5}).events.empty()); }
    void emptyIndex() { QVERIFY(IntervalIndex().query({0, 100}).events.empty()); }
    void invalidDuration() { QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IntervalIndex({{1, 0, -1, 0, 0}})); }
    void overflowRejected() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IntervalIndex({{1, std::numeric_limits<TimeNs>::max(), 1, 0, 0}}));
    }
    void queryCapReportsTruncation() {
        IntervalIndex index({{1, 0, 100, 0, 0}, {2, 1, 100, 0, 0}});
        auto q = index.query({50, 51}, 1);
        QCOMPARE(q.events.size(), std::size_t(1));
        QVERIFY(q.truncated);
    }
    void exactLimitNotTruncated() {
        auto q = IntervalIndex({{1, 0, 100, 0, 0}}).query({50, 51}, 1);
        QVERIFY(!q.truncated);
    }
    void indexMatchesBruteForce() {
        std::mt19937 rng(19);
        std::vector<Event> events;
        for (unsigned i = 0; i < 2000; ++i) events.push_back({i, rng() % 10000, 1 + rng() % 4000, 0, 0});
        IntervalIndex index(events);
        for (int r = 0; r < 100; ++r) {
            const TimeNs begin = rng() % 10000;
            TimeRange range{begin, begin + 1 + rng() % 1000};
            std::vector<std::uint64_t> expected, actual;
            for (const auto& e : events) if (e.start < range.end && e.end() > range.begin) expected.push_back(e.id);
            for (auto* e : index.query(range).events) actual.push_back(e->id);
            std::sort(expected.begin(), expected.end());
            std::sort(actual.begin(), actual.end());
            QVERIFY(expected == actual);
        }
    }
    void cancellationBeforeBuild() {
        auto flag = std::make_shared<std::atomic_bool>(true);
        QVERIFY_THROWS_EXCEPTION(Cancelled, generateTrace(1000000, 1, flag));
    }
    void invalidTrack() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, buildStore({{1, 0, 1, 9, 0}}, {"one"}, {"x"}, 1));
    }
    void immutableDeterministicSnapshot() {
        auto a = generateTrace(1000, 1), b = generateTrace(1000, 2);
        QCOMPARE(a->eventCount, std::size_t(1000));
        QCOMPARE(a->tracks[0].index.events()[1].duration, b->tracks[0].index.events()[1].duration);
    }
    void overviewCountsIntervalsNotOnlyStarts() {
        auto s = buildStore({{1, 0, 10000, 0, 0}}, {"one"}, {"x"}, 1);
        QVERIFY(s->tracks[0].overviewCounts[100] == 1);
    }
    void boundedDenseRendering() {
        auto s = generateTrace(100000, 1);
        auto batch = makeRenderBatch(*s, {1, s->bounds, 200, 0, 16});
        QVERIFY(batch.approximate);
        QVERIFY(batch.primitives.size() <= std::size_t(200 * 2 * 16));
    }
    void cacheHitAndInvalidation() {
        auto s = generateTrace(1000, 1);
        RenderCache cache;
        RenderKey key{1, s->bounds, 800, 0, 8};
        cache.get(s, key); cache.get(s, key);
        QCOMPARE(cache.hits(), std::uint64_t(1));
        ++key.width; cache.get(s, key);
        ++key.firstTrack; cache.get(s, key);
        key.range.end /= 2; cache.get(s, key);
        cache.get(generateTrace(1000, 1), key); // 即使版本相同，不同快照也不复用。
        QCOMPARE(cache.misses(), std::uint64_t(5));
    }
    void zoomKeepsAnchor() {
        TimeViewport view({0, 1000}); view.zoom(2, 0.25);
        QCOMPARE(view.range().begin, TimeNs(125));
        QCOMPARE(view.range().end, TimeNs(625));
    }
    void viewportClamps() {
        TimeViewport view({0, 1000}); view.zoom(2, 0.5); view.pan(100);
        QCOMPARE(view.range().end, TimeNs(1000)); view.pan(-100);
        QCOMPARE(view.range().begin, TimeNs(0));
    }
    void invalidZoomIgnored() {
        TimeViewport view({0, 1000}); view.zoom(0, 0.5);
        QCOMPARE(view.range().end, TimeNs(1000));
    }
    void clockUnknownIsNotZero() { QVERIFY(!ClockMapping().map(1234).has_value()); }
    void clockOffsetAndScale() {
        const auto clock = ClockMapping::fromAnchors({100, 1000}, {200, 3000}, 50);
        QCOMPARE(*clock.map(150), TimeNs(2000)); QCOMPARE(clock.uncertaintyNs(), TimeNs(50));
    }
    void invalidClockAnchors() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, ClockMapping::fromAnchors({100, 10}, {100, 20}, 0));
    }
    void mailboxBounded() {
        LatestRequest<int> box;
        for (int i = 0; i < 10000; ++i) box.replace(i);
        QCOMPARE(*box.take(), 9999); QVERIFY(!box.hasValue());
    }
    void controllerPublishesOnGuiThread() {
        SessionController controller;
        QSignalSpy spy(&controller, &SessionController::snapshotReady);
        QThread* receiverThread = nullptr;
        connect(&controller, &SessionController::snapshotReady, this, [&] { receiverThread = QThread::currentThread(); });
        controller.requestSynthetic(1000);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
        QCOMPARE(receiverThread, QCoreApplication::instance()->thread());
    }
    void latestRequestWins() {
        SessionController controller;
        controller.requestSynthetic(1000000);
        for (int i = 0; i < 100; ++i) controller.requestSynthetic(1000 + std::size_t(i));
        QVERIFY(controller.pendingRequests() <= 1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(controller.snapshot());
        QCOMPARE(controller.snapshot()->eventCount, std::size_t(1099));
    }
    void cancelRetainsSession() {
        SessionController controller;
        controller.requestSynthetic(1000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        auto previous = controller.snapshot();
        controller.requestSynthetic(1000000); controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(previous == controller.snapshot());
    }
    void failedBuildRetainsSession() {
        SessionController controller;
        controller.requestSynthetic(1000); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        auto previous = controller.snapshot();
        QSignalSpy errors(&controller, &SessionController::message);
        controller.requestSynthetic(0); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QCOMPARE(errors.count(), 1); QVERIFY(previous == controller.snapshot());
    }
    void destructorJoinsWorker() {
        QElapsedTimer timer; timer.start();
        { SessionController controller; controller.requestSynthetic(1000000); }
        QVERIFY(timer.elapsed() < 1000);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
