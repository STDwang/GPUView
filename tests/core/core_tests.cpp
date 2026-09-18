#include "core/clock_mapping.h"
#include "core/render_query.h"
#include "adapters/synthetic_source.h"
#include "application/session_controller.h"
#include <QtTest>
#include "adapters/presentmon_csv.h"
#include "core/statistics.h"
#include "application/statistics_controller.h"
#include <sstream>
#include <QTemporaryFile>
#include <limits>
#include <random>
using namespace gpuview;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void filteredTrackMappingInvalidatesCache() {
        auto data=buildStore({{1,0,10,0,0},{2,0,20,1,0}},{"a","b"},{"e"},1);
        RenderCache cache; RenderKey key{1,{0,20},100,0,1,{0}};
        QCOMPARE(cache.get(data,key).primitives.front().eventId,std::uint64_t(1));
        key.trackIds={1};
        QCOMPARE(cache.get(data,key).primitives.front().eventId,std::uint64_t(2));
        QCOMPARE(cache.misses(),std::uint64_t(2)); key.trackCount=0;
        QVERIFY(cache.get(data,key).primitives.empty());
    }

    void frameBaselineUsesFullHistory() {
        std::vector<Event> events;
        for(int i=0;i<40;++i) events.push_back({std::uint64_t(i+1),TimeNs(i)*50000000,50000000,0,0});
        events.push_back({41,2000000000,80000000,0,0});
        auto data=buildStore(events,{"frames"},{"frame"},1,{}, {},false,true);
        auto selected=calculateStatistics(*data,{2000000000,2080000000},{0});
        QCOMPARE(selected.count,std::size_t(1)); QCOMPARE(selected.longFrames,std::size_t(0));
        QCOMPARE(calculateStatistics(*data,{20,10},{0}).count,std::size_t(0));
    }
    void csvEscapesUnorderedAndCancellation() {
        std::istringstream in("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n\"demo\"\"app\nline\",1,x,2,10\n\"demo\"\"app\nline\",1,x,1,20\n");
        auto data=readPresentMon(in,1); QCOMPARE(data->tracks.size(),std::size_t(1));
        QCOMPARE(data->tracks[0].index.events()[0].duration,TimeNs(20000000));
        auto flag=std::make_shared<std::atomic_bool>(true); std::istringstream cancelled("unused");
        QVERIFY_THROWS_EXCEPTION(Cancelled,readPresentMon(cancelled,1,flag));
    }

    void realCaptureFixture() {
        const auto path=QFINDTESTDATA("../../data/samples/presentmon-real.csv"); QVERIFY(!path.isEmpty());
        auto data=loadPresentMon(std::filesystem::path(path.toStdWString()),1);
        QCOMPARE(data->eventCount,std::size_t(905));
        const auto stats=calculateStatistics(*data,data->bounds,{0});
        QVERIFY(std::abs(stats.meanMs-8.1924959116)<1e-8);
        QVERIFY(stats.longFrames>0); QCOMPARE(stats.longest->duration,TimeNs(92316100));
        QVERIFY(!data->tracks[0].frameMaxDuration.empty());
    }

    void csvQuotedBomInvalidAndGroups() {
        std::istringstream in("\xef\xbb\xbf" "Application,ProcessID,SwapChainAddress,TimeInSeconds,msBetweenPresents\r\n"
            "\"demo,one\",1,0x1,2,10\r\n\"demo,one\",1,0x1,2.01,20\r\nother,2,0x2,3,60\r\n"
            "bad,3,x,NaN,10\r\nbad,3,x,4,-1\r\n");
        auto data=readPresentMon(in,7);
        QCOMPARE(data->eventCount,std::size_t(3)); QCOMPARE(data->tracks.size(),std::size_t(2));
        QVERIFY(data->frames); QVERIFY(!data->synthetic);
        QCOMPARE(data->tracks[0].index.events()[0].start,TimeNs(0));
        QCOMPARE(data->tracks[0].index.events()[1].duration,TimeNs(20000000));
        QVERIFY(data->tracks[0].name.find("demo,one")!=std::string::npos);
        QVERIFY(data->warnings.size()>=3);
    }
    void csvRejectsUnsupportedAndBroken() {
        std::istringstream unsupported("Application,ProcessID,SwapChainAddress,CPUStartQPC,FrameTime\na,1,x,1,2\n");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(unsupported,1));
        std::istringstream broken("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n\"broken");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(broken,1));
        std::istringstream empty("");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(empty,1));
    }
    void exactStatisticsClipAndHalfOpen() {
        auto data=buildStore({{1,0,20,0,0},{2,10,20,0,0},{3,20,10,1,0}},{"a","b"},{"x"},1);
        auto stats=calculateStatistics(*data,{10,20},{0,1});
        QCOMPARE(stats.count,std::size_t(2)); QCOMPARE(stats.sumMs,20.0/1e6);
        QCOMPARE(stats.p95Ms,10.0/1e6); QCOMPARE(stats.longest->id,std::uint64_t(1));
        QCOMPARE(calculateStatistics(*data,{10,20},{}).count,std::size_t(0));
    }
    void frameStatisticsKnownValuesAndBoundary() {
        std::istringstream in("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\na,1,x,0,10\na,1,x,.01,10\na,1,x,.02,20\na,1,x,.04,60\n");
        auto data=readPresentMon(in,1); auto stats=calculateStatistics(*data,data->bounds,{0});
        QCOMPARE(stats.count,std::size_t(4)); QCOMPARE(stats.meanMs,25.0); QCOMPARE(stats.p50Ms,10.0);
        QCOMPARE(stats.p95Ms,60.0); QCOMPARE(stats.longFrames,std::size_t(1));
        QCOMPARE(stats.longest->duration,TimeNs(60000000));
        auto selected=calculateStatistics(*data,{10000000,40000000},{0});
        QCOMPARE(selected.count,std::size_t(2)); QCOMPARE(selected.sumMs,30.0);
    }
    void statisticsCancelAndLatestWins() {
        auto data=generateTrace(100000,1); auto flag=std::make_shared<std::atomic_bool>(true);
        QVERIFY_THROWS_EXCEPTION(Cancelled,calculateStatistics(*data,data->bounds,{0},flag));
        StatisticsController controller; QSignalSpy ready(&controller,&StatisticsController::ready);
        controller.request(data,data->bounds,{0,1,2,3});
        controller.request(data,{0,1},{});
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(),1,5000);
        QCOMPARE(controller.result().count,std::size_t(0));
        controller.request(data,data->bounds,{0}); controller.cancel();
        QTest::qWait(50); QCOMPARE(ready.count(),1);
    }
    void fileLoadFailurePreservesSnapshot() {
        SessionController controller; QSignalSpy ready(&controller,&SessionController::snapshotReady);
        QSignalSpy error(&controller,&SessionController::message);
        controller.requestSynthetic(100); QTRY_COMPARE_WITH_TIMEOUT(ready.count(),1,5000);
        auto previous=controller.snapshot();
        controller.requestFile("missing-gpuview-file.csv");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),5000);
        QCOMPARE(controller.snapshot(),previous); QVERIFY(error.count()>0);
    }

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
