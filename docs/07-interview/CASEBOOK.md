# 面试问题、解决思路与代码例子

版本：v0.1教学内核。以下区分“已实现并验证”“工程情景”“尚未接入”，不将为了说明而构造的情景写成生产事故。问题原文见 [十题清单](QUESTIONS.md)。

## Q01 如何在Qt中流畅显示百万级事件？

问题：数据量比屏幕像素多得多，全览时逐条画会浪费CPU，控件树还会放大内存和事件派发成本。

实现思路：紧凑Event数组 → 每轨区间索引 → 只查询可见轨道 → 查询结果设置上限 → 密集时使用预计算概览 → 每像素最多一个概览图元。鼠标点击仍回到原始事件，不把概览桶当精确事实。

代码入口：[TraceStore与IntervalIndex](../../src/core/trace_store.h)、[makeRenderBatch](../../src/core/render_query.cpp)、[TimelineWidget::paintEvent](../../src/ui_widgets/timeline_widget.cpp)。

```cpp
const auto exact = track.index.query(key.range, std::size_t(key.width) * 2);
if (!exact.truncated) {
    // 仅绘制查询命中的原始事件。
} else {
    out.approximate = true;
    // 用预计算4096桶投影到像素，概览复杂度与可见轨道/宽度相关。
}
```

验证：boundedDenseRendering检查图元有界；rendersBoundedGeometry验证Widget可绘制；百万事件查询基准附在 [BASELINE](../../benchmarks/reports/BASELINE.md)。

30秒回答：我将数据规模与绘制规模分开。原始数据保留百万条，绘制只关心可见轨道和当前像素分辨率；密集区域用LOD，放大后回到精确事件。当前基准证明了查询与几何构建收益，还没有证明所有硬件上稳定60FPS。

追问：4096桶是固定概览，极密集的窄选区可能仍显示粗粒度。后续可做分级聚合/后台精细化；不能拿本版近似桶计算P99。

## Q02 为什么不能给每个事件创建一个QGraphicsItem/QML Item？

这不是绝对禁止。几十/几百个复杂交互对象很适合Item体系，问题在“每个百万级事件一个对象”：除了业务数据，还要承受场景节点、包围盒、拾取、状态和更新管理。

本项目把大多数事件当数据，只有可见结果生成轻量Primitive，整块时间轴一个QWidget。代码：[Event/TraceStore](../../src/core/trace_store.h)、[Primitive](../../src/core/render_query.h)。

```cpp
struct Event {
    std::uint64_t id;
    TimeNs start;
    TimeNs duration;
    std::uint32_t track;
    std::uint32_t name;
};
```

名字采用ID查字典，不在每条事件上复制QString/QVariantMap。索引仍有额外内存，不能把结构体大小乘百万就当进程总占用。

验证：百万数据生成后eventCount为100万，当前基准全览16轨道仅20,336个图元。未实现“百万QGraphicsItem”对照实验，因此不编造那种实现慢多少倍。

30秒回答：我没有否定Graphics View，而是根据事件粒度选批量绘制，减少与屏幕无关的对象数量。复杂独立交互对象仍可使用Graphics View，取舍要看场景与实测。

## Q03 时间轴缩放时如何查询可见事件？

工程情景：长事件[0,1000)与视口[400,450)相交，但如果只找start落在视口内的事件，会漏掉它。

解决：事件按start排序，每棵子树记录maxEnd。只有子树所有事件都在视口左边，或其最早start在视口右边时才能剪枝。叶子命中遵循半开区间。

代码：[IntervalIndex::visit](../../src/core/trace_store.cpp)。

```cpp
if (out.truncated || maxEnd_[node] <= range.begin
    || events_[lo].start >= range.end) return;
```

验证：longIntervalCrossesViewport、halfOpenBoundaries，以及2000个随机事件×100个查询与朴素扫描逐ID对照。索引不能把所有查询都保证O(logN)：大量事件相交时必须承担输出成本，本项目另用limit和LOD控制渲染输出。

30秒回答：点数据可以按时间二分，区间事件还要考虑起点在左侧但没有结束的事件。我用排序数组加子树最大结束时间剪枝，并用跨界长事件测试防止漏查。

## Q04 如何减少高频信号导致的事件队列堆积？

工程情景：worker每处理一条事件都发progress，即使每条信号很轻，百万条队列通知也会让GUI处理过时状态。

解决：进度写入一个atomic<int>，GUI的100ms定时器读取最新值。重复构建请求只保留最新待办，同时取消旧任务。原始数据不丢弃，丢的是用户已不再需要的中间状态。

代码：[ProgressMailbox / SessionController](../../src/application/session_controller.h)、[LatestRequest](../../src/core/trace_store.h)。

```cpp
progress->percent.store(p, std::memory_order_relaxed);
// GUI定时器只读取最新值；完成通知仍使用queued connection。
if (worker_) {
    pending_.replace(request);
    cancel_->store(true, std::memory_order_relaxed);
}
```

验证：mailboxBounded写入10000次只取最后值；latestRequestWins连续请求100次后只发布最后一份数据，待办上限1。

30秒回答：限制通知频率和队列长度比单纯加线程更重要。我将进度变成最新状态邮箱，将耗时任务请求变成一个运行中加一个最新待办。实时数据流以后还需要持久化与背压策略，不能照搬“丢弃中间状态”去丢原始采样。

## Q05 Worker线程怎样安全地给UI提供数据？

问题：UI持有可变vector时，worker追加可能使引用失效或造成数据竞争。加一把长时间持有的锁又会卡住GUI。

解决：worker独占构建，完成后转为shared_ptr<const TraceStore>；只在GUI线程接收完成通知并切换快照。worker不捕获MainWindow/控制器this。

代码：[SessionController::start](../../src/application/session_controller.cpp)。

```cpp
worker_ = QThread::create([request, cancel, progress, result] {
    result->snapshot = generateTrace(request.count, request.generation, cancel, ...);
});
// 省略的部分是进度回调和异常处理，完整实现以链接文件为准。
```

完成回调显式QueuedConnection；wait确认完成同步后读取Result，随后核对generation再赋给current_。shared_ptr只解决共享所有权，不自动让可变对象线程安全，所以发布的类型必须const且发布后不再写。

验证：controllerPublishesOnGuiThread确认接收线程等于应用线程；latestRequestWins防止迟到结果覆盖。

30秒回答：我让worker生产不可变快照，GUI只在自己的线程交换引用和刷新视图。避免边读边写，不依赖跨线程直接调用控件。

## Q06 怎样实现取消、退出和对象生命周期管理？

问题：只有“取消按钮”但耗时循环不检查取消，不是真正可取消；窗口关闭时仍有线程访问对象会发生悬空引用或QThread仍在运行的错误。

解决：原子取消标志贯穿生成、分组、排序比较、索引构建和概览生成；失败/取消不替换旧会话。退出先停止进度定时器，发取消、断开回调、wait、再销毁线程。

代码：[checkCancelled](../../src/core/trace_store.h)、[构建取消点](../../src/core/trace_store.cpp)、[控制器析构](../../src/application/session_controller.cpp)。

```cpp
poll_.stop();
if (cancel_) cancel_->store(true, std::memory_order_relaxed);
if (worker_) {
    disconnect(worker_, nullptr, this, nullptr);
    worker_->wait();
    delete worker_;
}
```

验证：cancellationBeforeBuild、cancelRetainsSession、failedBuildRetainsSession、destructorJoinsWorker、closeWhileBuilding。

30秒回答：取消是协作协议，必须设计检查点与资源所有权，不能用terminate强杀。当前是纯CPU任务，检查点可控；未来文件I/O若可能阻塞，还要考虑超时/分块读取，否则wait仍可能长时间等待。

## Q07 QPainter、QGraphicsView、Qt Quick Scene Graph如何选择？

| 方案 | 合适的任务 | 本项目取舍 |
|---|---|---|
| QWidget/QPainter | Widgets集成、可控批量2D绘制 | 当前实现；自己处理坐标/拾取/LOD |
| QGraphicsView | 多种独立图元、复杂场景交互、内建选择变换 | 可以用于较少复杂对象；本项目未为每事件建Item |
| Qt Quick Scene Graph | QML界面、动画、批量几何及GPU渲染 | 后续对照实现；需要理解渲染线程与节点生命周期 |

代码：[TimelineWidget::paintEvent](../../src/ui_widgets/timeline_widget.cpp)里可指出裁剪、时间投影和fillRect；[makeRenderBatch](../../src/core/render_query.cpp)不依赖QPainter，未来可以给Quick消费。

```cpp
painter.setClipRect(gutter, top, plotWidth, height() - top);
for (const auto& primitive : batch.primitives) {
    // 只把已准备好的几何送给绘制API，paint里不解析文件。
}
```

30秒回答：先按项目现有技术栈、对象规模和交互需求选工具，再把数据查询与绘制API分离。当前选QPainter便于学习和Widgets交付；后续用相同数据测试Quick，不能只凭“GPU加速”就认定更快。

边界：尚无三个渲染框架的实测横向对比，不能声称QPainter在所有场景最优。

## Q08 什么情况下做缓存，缓存如何失效？

问题：视口没变，焦点/遮挡等触发重绘时，重复构建几何是浪费；缓存键漏了快照或宽度会让用户看到错误位置。

解决：缓存一份RenderBatch。key包括version、range、width、firstTrack、trackCount，并检查快照身份。缓存的是逻辑几何，不是包含颜色/字体的像素图。

代码：[RenderCache::get](../../src/core/render_query.cpp)、[RenderKey](../../src/core/render_query.h)。

```cpp
if (owner_ == store && key_ && *key_ == key) {
    ++hits_;
    return batch_;
}
batch_ = makeRenderBatch(*store, key);
```

验证：cacheHitAndInvalidation改变宽度、轨道、范围、快照，即使版本相同也会miss。后续加入过滤条件，必须把过滤版本放入key；否则是缺陷。

30秒回答：只缓存昂贵且输入稳定的计算。关键不是有缓存容器，而是明确结果依赖哪些输入、如何限制大小和何时失效。本版只缓存一项，内存有界；若改缓存栅格图，主题和DPR也必须成为失效因素。

## Q09 CPU Timeline和GPU Timeline如何对齐？

问题：两个源的“0秒”可能指不同开始时刻，时钟频率也可能不同。把x轴单位都写ms并不能说明对齐。

算法：明确source clock与session clock，用同次采集的锚点构建仿射映射t_session = offset + scale×t_source。保存误差；未知锚点返回nullopt。真实GPU时钟映射需要采集工具提供关联校准，不能从游戏CSV猜测。

代码：[ClockMapping](../../src/core/clock_mapping.cpp)。

```cpp
result.scale_ = (static_cast<long double>(b.sessionNs) - a.sessionNs)
              / (static_cast<long double>(b.sourceTick) - a.sourceTick);
// 先减原点再缩放，避免直接放大绝对时间。
```

验证：clockOffsetAndScale使用(100→1000ns, 200→3000ns)，150映射到2000ns；clockUnknownIsNotZero保证未知不伪装成0；invalidClockAnchors拒绝错误锚点。

30秒回答：我先确认时钟域和基准，再做单位/频率/偏移转换，并保留对齐误差。当前只实现了映射算法和教学验证，还没有真实CPU/GPU双源采集集成；真实系统中漂移、驱动校准和采样不确定性仍要处理。

## Q10 如何用数据证明一次UI性能优化有效？

本次可复现问题：大量事件的窄范围查询如果每次全量扫描，其时间随总事件数增加。实现了朴素扫描和区间索引两条路径，用同一seed、同一数据、同一100个范围检查命中数量一致，再比较耗时。

代码：[benchmark_main.cpp](../../tools/benchmark/main.cpp)。

```cpp
if (naiveCount != indexedCount) {
    std::cerr << "Correctness mismatch\n";
    return 2;
}
```

每个规模预热1次、正式5次，保存所有原始行；查询基准不含磁盘读写或绘制。百万事件下100查询中位数189.406ms和2.7329ms，结果详见 [报告](../../benchmarks/reports/BASELINE.md)。不是“历史版本提升69倍”的证据，而是同一个程序内两种算法对照。

30秒回答：先证明结果正确，再固定环境与操作序列重复测量，保留原始结果，分别报告CPU、内存、交互延迟。当前证明的是查询算法收益，完整UI收益还需输入到绘制P95、真实窗口绘制和长时间回放测试。

## 本次实际遇到的实现问题

### 离屏截图里中文和英文都变成方框

现象：第一次自动渲染能画图形，但所有文字显示方框。判断不是UTF-8源文件损坏，因为系统字体未在Windows离屏后端自动枚举。

处理：在 [main.cpp](../../src/app/main.cpp) 中通过QFontDatabase读取本机系统字体并设置应用字体，不把系统字体文件打包公开。重新构建、运行离屏截图并目视检查，中文与数字显示正常。

### 时间轴最后一个刻度被右侧裁切

现象：渲染截图右侧末端标签只有前几位可见。处理：按fontMetrics获取标签宽度，将文字起点夹在绘图边界内。位置：[TimelineWidget::paintEvent](../../src/ui_widgets/timeline_widget.cpp)。这说明“程序能启动”不足以完成自绘界面的验收，需要实际看输出。

### Qt Test可执行程序没有控制台输出

现象：CTest返回通过，但详细日志为空，不便保存可审查证据。先显式设置WIN32_EXECUTABLE FALSE，当前执行环境仍没有捕获到标准输出；最终让Qt Test使用`-o 文件,txt`写日志，构建脚本读取日志并保留原退出码。位置：[CMakeLists.txt](../../CMakeLists.txt)、[build.ps1](../../tools/build.ps1)。确认日志包含全部用例结果，而不只依赖进程返回0。

## 后续必须补充的真实问题

### 补充：新增源码后VS仍使用旧文件列表

现象：直接在正在执行的MSBuild中通过CONFIGURE_DEPENDS触发重新生成，当前已加载的工程列表可能仍是旧版本；删除文件时会尝试编译不存在的旧路径。

处理：两个IDE统一先由build.ps1显式运行CMake配置，再启动MSBuild，而非只调用`cmake --build`。模块列表通过 [CollectSources.cmake](../../cmake/CollectSources.cmake) 按目录收集，VS浏览列表由 [sync-vs-sources.ps1](../../tools/sync-vs-sources.ps1)自动生成相对路径。新增/删除临时文件均实际验证过。这既满足无需手工列文件，也避免构建器缓存旧图。

### 补充：Debug部署导致离屏插件找不到

现象：windeployqt把运行库部署到exe旁后，Qt从部署目录搜索插件；桌面程序所需Windows插件已复制，但离屏测试插件没有部署，导致GUI测试启动失败。

处理：应用保留默认桌面插件，CTest为widget_interaction单独指定SDK平台插件目录。Debug和Release日志分别命名，运行前清空，避免失败时误读旧配置通过日志。代码见 [CMakeLists.txt](../../CMakeLists.txt) 和 [build.ps1](../../tools/build.ps1)。

PresentMon版本字段差异、缺失帧统计、多源对齐、异步范围统计、完整回放背压和真实UI性能基准尚未实现。实现后将真实复现、修复与测试补入本文件；不能把以上教学原型当完整面试项目已经完成。
