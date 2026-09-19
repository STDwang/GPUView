/// @file tests/core/core_tests.cpp
/// @brief 核心算法、数据适配与异步生命周期回归；每个测试说明一个可观察契约，不以测试数量代替性能证据。
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
#include <QTemporaryDir>
#include <QBuffer>
#include <QCryptographicHash>
#include "core/frame_analysis.h"
#include "application/export_controller.h"
#include <limits>
#include <random>
using namespace gpuview;

/// Qt Test核心回归集合；无业务成员，每个private slot是独立测试入口。
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    /// 验证排序不改变事件身份、ID查找正确、稀疏热力桶保留尖峰且空选区不删除全会话热图。
    void frameRowsSortIdentityAndSparseHeat() {
        auto store=buildStore({{9,0,2000000,0,0},{3,500000000,10000000,0,0},{7,2000000000,60000000,0,0}},
            {"app"},{"frame"},1,{}, {},false,true);
        auto analysis=analyzeFrames(store,{0,1000000000},{0},FrameSort::Duration,true);
        QCOMPARE(analysis->rows.size(),std::size_t(2)); QCOMPARE(analysis->event(0).id,std::uint64_t(3));
        QCOMPARE(analysis->rowForId(9),1); QCOMPARE(analysis->rowForId(7),-1);
        QCOMPARE(analysis->heat.size(),std::size_t(2)); QCOMPARE(analysis->heat[0].count,std::size_t(2));
        QCOMPARE(analysis->heat[0].maximum,TimeNs(10000000)); QCOMPARE(analysis->heat[1].begin,TimeNs(2000000000));
        QCOMPARE(analysis->heatOverview.size(),std::size_t(4096)); QCOMPARE(analysis->heatOverview[2500],TimeNs(0));
        auto empty=analyzeFrames(store,{1000000000,2000000000},{0}); QCOMPARE(empty->summary.count,std::size_t(0));
        QVERIFY(!empty->heat.empty()); // 全会话热力图不能因选中空区间而丢失其他时间数据。
    }
    /// 把公开CSV的独立SHA-256与解析器摘要对照，并检查有效/排除记录计数。
    void importedBytesHaveMatchingHashAndQuality() {
        const auto path=QFINDTESTDATA("../../data/samples/presentmon-real.csv"); QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto hash=QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex();
        auto store=loadPresentMon(std::filesystem::path(path.toStdWString()),1);
        QCOMPARE(QString::fromStdString(store->input.sha256),QString::fromLatin1(hash));
        QCOMPARE(store->input.records,std::size_t(905)); QCOMPARE(store->input.rejected,std::size_t(0));
    }
    /// 验证报告来源元数据、CSV引号/公式处理、Markdown转义和空统计N/A。
    void exportMetadataEscapingAndEmptyValues() {
        auto store=buildStore({{5,0,2000000,0,0}},{"app,\"x\""},{"frame"},1,{}, {},false,true,"source",{}, {"abcd",2,1});
        auto analysis=analyzeFrames(store,{0,3000000},{0}); QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly));
        writeAnalysis(buffer,*analysis,ExportFormat::FramesCsv,QStringLiteral("=测试,\"引号\"\n下一行"));
        QVERIFY(bytes.contains("\"source_sha256\",\"abcd\"")); QVERIFY(bytes.contains("\"source_rejected\",\"1\""));
        QVERIFY(bytes.contains("frame,,,5,0,2000000,0")); QVERIFY(bytes.contains("\"\"")); QVERIFY(bytes.contains("\"'="));
        auto empty=analyzeFrames(store,{3000000,4000000},{0}); bytes.clear(); buffer.seek(0);
        writeAnalysis(buffer,*empty,ExportFormat::Markdown,QStringLiteral("<img>|[link]"));
        QVERIFY(bytes.contains("N/A")); QVERIFY(bytes.contains("&lt;img&gt;")); QVERIFY(!bytes.contains("<img>"));
    }
    /// 在完整帧导出途中取消，断言旧报告完整、临时文件清理，并覆盖成功替换和无效目标失败。
    void exportCancelMidWritePreservesOriginal() {
        std::vector<Event> events; for(int i=0;i<3000;++i) events.push_back({std::uint64_t(i+1),TimeNs(i)*1000000,1000000,0,0});
        auto source=buildStore(events,{"app"},{"frame"},1,{}, {},true,true);
        auto analysis=analyzeFrames(source,source->bounds,{0}); QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto path=directory.filePath("report.csv"); QFile original(path); QVERIFY(original.open(QIODevice::WriteOnly)); original.write("previous report"); original.close();
        auto cancel=std::make_shared<std::atomic_bool>(false); bool wrotePart=false;
        // 进度进入中段时主动置取消，保证测试覆盖“已写部分内容”而非只测启动前取消。
        QVERIFY_THROWS_EXCEPTION(Cancelled,saveAnalysis(path,*analysis,ExportFormat::FramesCsv,{},cancel,[&](int percent) {
            if(percent>0 && percent<99) { wrotePart=true; cancel->store(true); }
        }));
        QVERIFY(wrotePart); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),QByteArray("previous report")); original.close();
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files|QDir::Hidden).size(),1);
        saveAnalysis(path,*analysis,ExportFormat::SummaryCsv,{}); QVERIFY(original.open(QIODevice::ReadOnly)); QVERIFY(original.readAll().startsWith("record_type"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,saveAnalysis(directory.filePath("missing/report.csv"),*analysis,ExportFormat::SummaryCsv,{}));
    }
    /// 导出后改变调用方选区，断言报告仍使用发起时快照，同时验证当前输入覆盖被拒绝。
    void exportControllerUsesFrozenSnapshotAndProtectsInput() {
        auto source=buildStore({{1,0,100,0,0},{2,100,100,0,0}},{"app"},{"frame"},1,{}, {},false,true);
        auto first=analyzeFrames(source,{0,100},{0}); QTemporaryDir directory; const auto path=directory.filePath("report.csv");
        ExportController controller; QSignalSpy done(&controller,&ExportController::completed);
        QVERIFY(controller.request(path,first,ExportFormat::FramesCsv,{})); first=analyzeFrames(source,{100,200},{0});
        QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,5000); QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); auto bytes=file.readAll(); file.close();
        QVERIFY(bytes.contains("frame,,,1,0,100,0")); QVERIFY(!bytes.contains("frame,,,2,"));
        QVERIFY(!controller.request(path,first,ExportFormat::FramesCsv,{},path));
    }

    /// 在版本和时间不变时切换轨道映射，验证几何不错误命中旧缓存。
    void filteredTrackMappingInvalidatesCache() {
        auto data=buildStore({{1,0,10,0,0},{2,0,20,1,0}},{"a","b"},{"e"},1);
        RenderCache cache; RenderKey key{1,{0,20},100,0,1,{0}};
        QCOMPARE(cache.get(data,key).primitives.front().eventId,std::uint64_t(1));
        key.trackIds={1};
        QCOMPARE(cache.get(data,key).primitives.front().eventId,std::uint64_t(2));
        QCOMPARE(cache.misses(),std::uint64_t(2)); key.trackCount=0;
        QVERIFY(cache.get(data,key).primitives.empty());
    }

    /// 选择局部范围仍使用此前完整历史判长帧，避免选区改变导致基线漂移。
    void frameBaselineUsesFullHistory() {
        std::vector<Event> events;
        for(int i=0;i<40;++i) events.push_back({std::uint64_t(i+1),TimeNs(i)*50000000,50000000,0,0});
        events.push_back({41,2000000000,80000000,0,0});
        auto data=buildStore(events,{"frames"},{"frame"},1,{}, {},false,true);
        auto selected=calculateStatistics(*data,{2000000000,2080000000},{0});
        QCOMPARE(selected.count,std::size_t(1)); QCOMPARE(selected.longFrames,std::size_t(0));
        QCOMPARE(calculateStatistics(*data,{20,10},{0}).count,std::size_t(0));
    }
    /// 覆盖CSV引号转义、无序输入归一化和预先取消路径。
    void csvEscapesUnorderedAndCancellation() {
        std::istringstream in("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n\"demo\"\"app\nline\",1,x,2,10\n\"demo\"\"app\nline\",1,x,1,20\n");
        auto data=readPresentMon(in,1); QCOMPARE(data->tracks.size(),std::size_t(1));
        QCOMPARE(data->tracks[0].index.events()[0].duration,TimeNs(20000000));
        auto flag=std::make_shared<std::atomic_bool>(true); std::istringstream cancelled("unused");
        QVERIFY_THROWS_EXCEPTION(Cancelled,readPresentMon(cancelled,1,flag));
    }

    /// 用公开实采夹具验证905条记录、关键统计与来源限制。
    void realCaptureFixture() {
        const auto path=QFINDTESTDATA("../../data/samples/presentmon-real.csv"); QVERIFY(!path.isEmpty());
        auto data=loadPresentMon(std::filesystem::path(path.toStdWString()),1);
        QCOMPARE(data->eventCount,std::size_t(905));
        const auto stats=calculateStatistics(*data,data->bounds,{0});
        QVERIFY(std::abs(stats.meanMs-8.1924959116)<1e-8);
        QVERIFY(stats.longFrames>0); QCOMPARE(stats.longest->duration,TimeNs(92316100));
        QVERIFY(!data->tracks[0].frameMaxDuration.empty());
    }

    /// 验证BOM/引号字段、坏记录计数和进程交换链分组。
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
    /// 拒绝不支持字段模式和破损CSV，避免静默补零或误读。
    void csvRejectsUnsupportedAndBroken() {
        std::istringstream unsupported("Application,ProcessID,SwapChainAddress,CPUStartQPC,FrameTime\na,1,x,1,2\n");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(unsupported,1));
        std::istringstream broken("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n\"broken");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(broken,1));
        std::istringstream empty("");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,readPresentMon(empty,1));
    }
    /// 验证Trace与半开选区相交时长裁剪和精确统计边界。
    void exactStatisticsClipAndHalfOpen() {
        auto data=buildStore({{1,0,20,0,0},{2,10,20,0,0},{3,20,10,1,0}},{"a","b"},{"x"},1);
        auto stats=calculateStatistics(*data,{10,20},{0,1});
        QCOMPARE(stats.count,std::size_t(2)); QCOMPARE(stats.sumMs,20.0/1e6);
        QCOMPARE(stats.p95Ms,10.0/1e6); QCOMPARE(stats.longest->id,std::uint64_t(1));
        QCOMPARE(calculateStatistics(*data,{10,20},{}).count,std::size_t(0));
    }
    /// 用可手算帧数据验证均值/分位数及Present时间的半开边界归属。
    void frameStatisticsKnownValuesAndBoundary() {
        std::istringstream in("Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\na,1,x,0,10\na,1,x,.01,10\na,1,x,.02,20\na,1,x,.04,60\n");
        auto data=readPresentMon(in,1); auto stats=calculateStatistics(*data,data->bounds,{0});
        QCOMPARE(stats.count,std::size_t(4)); QCOMPARE(stats.meanMs,25.0); QCOMPARE(stats.p50Ms,10.0);
        QCOMPARE(stats.p95Ms,60.0); QCOMPARE(stats.longFrames,std::size_t(1));
        QCOMPARE(stats.longest->duration,TimeNs(60000000));
        auto selected=calculateStatistics(*data,{10000000,40000000},{0});
        QCOMPARE(selected.count,std::size_t(2)); QCOMPARE(selected.sumMs,30.0);
    }
    /// 快速替换统计请求并取消，断言只有最新有效结果被发布。
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
    /// 文件加载失败后旧成功快照保持可用，不留下空白会话。
    void fileLoadFailurePreservesSnapshot() {
        SessionController controller; QSignalSpy ready(&controller,&SessionController::snapshotReady);
        QSignalSpy error(&controller,&SessionController::message);
        controller.requestSynthetic(100); QTRY_COMPARE_WITH_TIMEOUT(ready.count(),1,5000);
        auto previous=controller.snapshot();
        controller.requestFile("missing-gpuview-file.csv");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),5000);
        QCOMPARE(controller.snapshot(),previous); QVERIFY(error.count()>0);
    }

    /// 验证起点早于视口但终点进入视口的长事件仍可查到。
    void longIntervalCrossesViewport() {
        IntervalIndex index({{1, 0, 1000, 0, 0}, {2, 100, 5, 0, 0}, {3, 500, 10, 0, 0}});
        auto q = index.query({400, 450});
        QCOMPARE(q.events.size(), std::size_t(1));
        QCOMPARE(q.events.front()->id, std::uint64_t(1));
    }
    /// 验证结束恰等于左边界、开始恰等于右边界的事件不算相交。
    void halfOpenBoundaries() {
        IntervalIndex index({{1, 10, 10, 0, 0}, {2, 20, 10, 0, 0}});
        auto q = index.query({20, 21});
        QCOMPARE(q.events.size(), std::size_t(1));
        QCOMPARE(q.events.front()->id, std::uint64_t(2));
    }
    /// 空时间区间必须返回空结果，不产生伪命中。
    void emptyRange() { QVERIFY(IntervalIndex({{1, 0, 10, 0, 0}}).query({5, 5}).events.empty()); }
    /// 空事件索引可安全查询且结果为空。
    void emptyIndex() { QVERIFY(IntervalIndex().query({0, 100}).events.empty()); }
    /// 验证负时长被拒绝，避免非法区间进入索引。
    void invalidDuration() { QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IntervalIndex({{1, 0, -1, 0, 0}})); }
    /// 验证start加duration溢出被拒绝，避免剪枝和绘制使用错误终点。
    void overflowRejected() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IntervalIndex({{1, std::numeric_limits<TimeNs>::max(), 1, 0, 0}}));
    }
    /// 命中超过查询上限时返回有限结果并明确设置截断标志。
    void queryCapReportsTruncation() {
        IntervalIndex index({{1, 0, 100, 0, 0}, {2, 1, 100, 0, 0}});
        auto q = index.query({50, 51}, 1);
        QCOMPARE(q.events.size(), std::size_t(1));
        QVERIFY(q.truncated);
    }
    /// 命中恰等于上限时不误报截断，区别“达到容量”和“省略额外记录”。
    void exactLimitNotTruncated() {
        auto q = IntervalIndex({{1, 0, 100, 0, 0}}).query({50, 51}, 1);
        QVERIFY(!q.truncated);
    }
    /// 同批范围对照朴素扫描与索引结果，验证剪枝不会漏查或多查。
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
    /// 预置取消标志后构建应抛Cancelled，不发布半成品。
    void cancellationBeforeBuild() {
        auto flag = std::make_shared<std::atomic_bool>(true);
        QVERIFY_THROWS_EXCEPTION(Cancelled, generateTrace(1000000, 1, flag));
    }
    /// 拒绝超出轨道列表的事件轨道ID。
    void invalidTrack() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, buildStore({{1, 0, 1, 9, 0}}, {"one"}, {"x"}, 1));
    }
    /// 验证固定seed生成可复现数据，并以const快照共享发布。
    void immutableDeterministicSnapshot() {
        auto a = generateTrace(1000, 1), b = generateTrace(1000, 2);
        QCOMPARE(a->eventCount, std::size_t(1000));
        QCOMPARE(a->tracks[0].index.events()[1].duration, b->tracks[0].index.events()[1].duration);
    }
    /// 概览计数包含跨桶区间，不能只计事件起点所在桶。
    void overviewCountsIntervalsNotOnlyStarts() {
        auto s = buildStore({{1, 0, 10000, 0, 0}}, {"one"}, {"x"}, 1);
        QVERIFY(s->tracks[0].overviewCounts[100] == 1);
    }
    /// 百万密集事件全览时图元数量受LOD约束，避免逐事件绘制。
    void boundedDenseRendering() {
        auto s = generateTrace(100000, 1);
        auto batch = makeRenderBatch(*s, {1, s->bounds, 200, 0, 16});
        QVERIFY(batch.approximate);
        QVERIFY(batch.primitives.size() <= std::size_t(200 * 2 * 16));
    }
    /// 验证相同键命中，以及影响几何的条件变化触发重建。
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
    /// 缩放前后鼠标锚点对应时刻近似不变，误差仅为整数时间舍入。
    void zoomKeepsAnchor() {
        TimeViewport view({0, 1000}); view.zoom(2, 0.25);
        QCOMPARE(view.range().begin, TimeNs(125));
        QCOMPARE(view.range().end, TimeNs(625));
    }
    /// 缩放和平移始终夹紧到有效全会话边界。
    void viewportClamps() {
        TimeViewport view({0, 1000}); view.zoom(2, 0.5); view.pan(100);
        QCOMPARE(view.range().end, TimeNs(1000)); view.pan(-100);
        QCOMPARE(view.range().begin, TimeNs(0));
    }
    /// 非法缩放参数不改变有效视口。
    void invalidZoomIgnored() {
        TimeViewport view({0, 1000}); view.zoom(0, 0.5);
        QCOMPARE(view.range().end, TimeNs(1000));
    }
    /// 未建立时钟映射返回空值，不能伪装成会话零时刻。
    void clockUnknownIsNotZero() { QVERIFY(!ClockMapping().map(1234).has_value()); }
    /// 用已知两点验证偏移与比例转换。
    void clockOffsetAndScale() {
        const auto clock = ClockMapping::fromAnchors({100, 1000}, {200, 3000}, 50);
        QCOMPARE(*clock.map(150), TimeNs(2000)); QCOMPARE(clock.uncertaintyNs(), TimeNs(50));
    }
    /// 验证反向/重复或非法时钟锚点被拒绝。
    void invalidClockAnchors() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, ClockMapping::fromAnchors({100, 10}, {100, 20}, 0));
    }
    /// 连续替换请求后只保留最新一个，验证取出后邮箱变空。
    void mailboxBounded() {
        LatestRequest<int> box;
        for (int i = 0; i < 10000; ++i) box.replace(i);
        QCOMPARE(*box.take(), 9999); QVERIFY(!box.hasValue());
    }
    /// 验证后台构建结果回到GUI线程发布，避免Worker直接更新UI。
    void controllerPublishesOnGuiThread() {
        SessionController controller;
        QSignalSpy spy(&controller, &SessionController::snapshotReady);
        QThread* receiverThread = nullptr;
        // 在接收回调记录实际线程，后续断言发布确实发生于GUI线程。
        connect(&controller, &SessionController::snapshotReady, this, [&] { receiverThread = QThread::currentThread(); });
        controller.requestSynthetic(1000);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
        QCOMPARE(receiverThread, QCoreApplication::instance()->thread());
    }
    /// 连续加载请求只允许最后有效代次发布。
    void latestRequestWins() {
        SessionController controller;
        controller.requestSynthetic(1000000);
        for (int i = 0; i < 100; ++i) controller.requestSynthetic(1000 + std::size_t(i));
        QVERIFY(controller.pendingRequests() <= 1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(controller.snapshot());
        QCOMPARE(controller.snapshot()->eventCount, std::size_t(1099));
    }
    /// 取消新加载后保留旧快照，并拒绝迟到结果覆盖。
    void cancelRetainsSession() {
        SessionController controller;
        controller.requestSynthetic(1000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        auto previous = controller.snapshot();
        controller.requestSynthetic(1000000); controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(previous == controller.snapshot());
    }
    /// 新构建失败时保留已发布会话，不把错误状态当有效数据。
    void failedBuildRetainsSession() {
        SessionController controller;
        controller.requestSynthetic(1000); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        auto previous = controller.snapshot();
        QSignalSpy errors(&controller, &SessionController::message);
        controller.requestSynthetic(0); QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QCOMPARE(errors.count(), 1); QVERIFY(previous == controller.snapshot());
    }
    /// 运行期间销毁控制器，验证取消并等待线程结束的生命周期边界。
    void destructorJoinsWorker() {
        QElapsedTimer timer; timer.start();
        { SessionController controller; controller.requestSynthetic(1000000); }
        QVERIFY(timer.elapsed() < 1000);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
