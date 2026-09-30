# 事件搜索与名称汇总：面试讲解

v0.4，2026-09-25。产品参考与边界见[Nsight对照说明](../02-product/NSIGHT_REFERENCE.md)。

## Q01/Q02：百万事件怎样提供可搜索的完整表格？

问题：绘制使用LOD，但搜索不能只搜屏幕上的几千个聚合图元，否则结果会随缩放而改变。

方案：[analyzeEvents](../../src/core/event_analysis.cpp)扫描只读源事件，先对名称字典计算匹配，再筛轨道/时间/完整时长，同时收集EventMatch和NameSummary。EventAnalysis共享持有source，结果指针不悬空。[EventAnalysisModel](../../src/ui_widgets/event_explorer.cpp)只为可见单元格格式化，不创建百万QTableWidgetItem。

事件结果保留完整命中，排序后另建ID到行号的二分映射。测试eventSearchFullRowsSortAndCancellation验证10021条结果未被常见的10000查询上限截断；ModelTester覆盖平面模型契约。百万合成事件搜索基准见[原始测量](../../benchmarks/reports/EVENT_ANALYSIS.md)，不是实际GPU采集或屏幕FPS。

不足：筛选变化仍完整重算；2026-09-30已增加[相同筛选排序复用](UI_REFINEMENT.md)，行引用及ID表仍占O(N)空间；名称种类很大时聚合成本也会增长，不能把四个教学名称的测量推广到任意数据。

## Q04/Q05/Q06：快速输入时如何避免旧搜索覆盖新条件？

复现：先搜全部，再快速输入不存在的名称，接着取消或关闭窗口。

方案分两层：[EventExplorer::schedule](../../src/ui_widgets/event_explorer.cpp)立即取消旧代次并清空旧行，180ms防抖合并输入；[EventAnalysisController](../../src/application/event_analysis_controller.cpp)只允许一运行和一最新待办。Worker捕获快照/参数，不访问UI，GUI完成回调检查generation后才发布；析构协作取消并wait。

代码路径：

    controller_.cancel();          // 先使旧结果无效
    debounce_.start();             // 等输入稍稳定再发请求
    // controller内部：运行中只替换pending_，绝不每次创建新线程

仅防抖不够：等待期间旧Worker仍可能完成，因此代次检查是正确性机制，防抖只是减少无效工作。

测试eventSearchLatestCancelAndShutdown验证最新结果、GUI接收线程、取消不再发布和析构等待；eventExplorerSearchSortAndNavigation验证表格排序恢复ID、筛选、空结果禁用导航。尚未做30分钟压力或取消响应P95测量。

## 业务追问：为什么时长筛选和贡献时长不是同一个值？

例子：Trace事件[0,100)ns，选区[60,100)ns，原始完整时长100ns，范围贡献40ns。时长过滤使用100ns，选区汇总使用40ns；否则同一事件会仅因为框选大小变化而跨越“完整时长”的筛选阈值。

帧不同：Present在选区内就统计完整前一间隔，不能把帧间隔裁剪成“选区内GPU执行时间”。并发Trace总和可以大于墙钟；名称分组不是调用栈树或实际GPU Kernel分类。测试eventSearchAggregationAndFrameSemantics用手算数据覆盖这些规则。

额外边界：按最大贡献排序直接比较整数纳秒，避免Windows下long double精度等同double时把2^53附近相邻整数误判为相等，测试包含此边界。

## 练习

1. 把代次判断临时移除，解释为何防抖无法阻止旧结果发布，再恢复并运行测试。
2. 用两条跨选区且重叠的事件手算贡献总和，解释为何它不是利用率。
3. 沿eventSelected / eventActivated / selectId说明单击、双击和反向选择为何不会信号递归。
