# 从真实帧到可讲解代码：P0/P1案例

这些是本次实现中面对的工程问题和验证情景，不是虚构的生产事故。配合 [十题清单](QUESTIONS.md) 阅读。

## Q01/Q03：百万事件的“显示”和“统计”为什么分开？

复现：生成100万教学事件，全览只看到概览色带；框选一段时间，统计页却要返回准确的数量与P95。

问题：LOD桶记录的是粗范围相交数；同一长事件可计入多个桶。拿桶相加当事件数会重复计数，用像素宽度算时长分布也不准确。

解决：[render_query.cpp](../../src/core/render_query.cpp) 只生成可见区图元，[statistics.cpp](../../src/core/statistics.cpp) 在后台读取原始事件。教学Trace用相交裁剪时长，帧用Present时刻筛选后的完整间隔。

```cpp
if (store.frames ? e.start < range.begin : e.end() <= range.begin) continue;
const auto duration = store.frames ? e.duration
    : std::min(e.end(), range.end) - std::max(e.start, range.begin);
```

证据：exactStatisticsClipAndHalfOpen验证两个重叠事件的裁剪计数和求和；frameStatisticsKnownValuesAndBoundary验证[10,10,20,60]ms的均值25ms、P50=10ms、P95=60ms；渲染图像的缓存开关对照相同。

30秒口述：我让显示有界、分析精确。绘制可以按像素合并，统计必须回到原始数据，并明确不同来源的选择口径。代价是大范围统计需要后台任务，不能直接在paintEvent里做。

追问：当前统计顺序扫描所选轨道并排序求分位数，时间/内存与命中量有关。工作线程保障交互与取消，但并不让算法成本消失；进一步可做精确索引聚合或可合并的近似分位数，并明确误差。

## Q04/Q05/Q06：快速重复框选，怎样防止旧结果覆盖新选区？

复现：连续框选A、B，再过滤掉轨道或切换数据。A的统计可能晚于B完成。

解决：[StatisticsController](../../src/application/statistics_controller.cpp) 最多一个运行任务、一个最新待执行请求。数据用不可变Snapshot持有；取消推进generation，结果只在GUI线程验证代次后发布。换数据、清除选择、过滤都会取消旧统计。

```cpp
if (generation == generation_) {
    if (error->isEmpty()) { result_ = *value; emit ready(); }
    else emit failed(*error);
}
if (auto next = pending_.take()) start(std::move(*next));
```

证据：statisticsCancelAndLatestWins验证只有最后请求发布、取消结果不会回来；控制器析构取消并wait。文件导入也复用SessionController的相同策略，失败保留旧会话。

30秒口述：取消是降低浪费，代次校验才保证结果不会错位。我不让worker碰UI，也不为每次拖动创建无界线程；最多保留一个最新请求。关闭时先结束生产者再释放接收者。

不足：底层同步文件读取不能中断正在执行的操作系统I/O调用；本地文件解析、建索引和排序都有取消点，网络盘或异常存储设备上的关闭等待仍需单独验证。

## Q03/Q08：过滤轨道后，为何不能继续复用旧几何？

复现：只显示CPU后切换为GPU，首行仍然是0、窗口宽度也没变。

问题：如果缓存键只有firstTrack和trackCount，过滤前后可能误命中旧的轨道图元；像素行号也不能再当原始轨道ID。

解决：RenderKey增加有序trackIds；TimelineWidget维护显示行→原始轨道映射，绘制和拾取使用同一映射。GUI只为可见行建立小型行位置表，原始数据不复制。

证据：filteredTrackMappingInvalidatesCache验证同位置换trackIds会失效；navigationFilterAndGroupCollapse验证过滤、折叠、恢复；eventSelectionAndZoomHistory验证过滤后拾取的原始track仍正确。

30秒口述：缓存键必须描述影响输出的全部输入。过滤是视图状态，不需要改原始数据，但必须进缓存键；显示顺序和数据身份应分离。

## 真实帧业务问题：长帧是否就是GPU慢？

复现：导入 [真实样本](../../data/samples/presentmon-real.csv)，在统计页双击长帧，观察帧曲线和事件详情。

来源：PresentMon 1.9.2采集本项目D3D11程序，每60帧主动等待70ms。[采集记录](../../data/samples/PRESENTMON_CAPTURE.md) 保存工具哈希、字段、匿名化方式和限制。905条记录的平均间隔约8.1925ms，最长92.3161ms。

解释：MsBetweenPresents描述相邻Present调用间隔。这里包含人为CPU等待、调度和Present行为，不能单凭这个值断言GPU执行慢。帧矩形起点是当前Present时刻，宽度编码前一间隔，不能把右边界当真实GPU完成时间。

实现：[presentmon_csv.cpp](../../src/adapters/presentmon_csv.cpp) 流式解析、检查单位/溢出、分组、记录无效数据。字段不认识就拒绝，不用0冒充缺失；帧图按每像素最大值保留尖峰，过密时使用预计算桶最大值并标注概览。

长帧规则：在完整进程/交换链历史中，用60FPS预算的两倍和最近至多120个有效间隔中位数的两倍取较大值。只筛选可疑帧，不作因果诊断。frameBaselineUsesFullHistory验证缩小选区不会重置基线。

证据：realCaptureFixture核验真实样本统计；CSV测试覆盖引号、BOM、跨行、坏数值、无序、缺列和取消；realFileAnalysisAndLongFrameNavigation验证文件→UI统计→长帧跳转。

## Q10：怎么证明缓存优化有效？

假设：悬停只变交互覆盖层，不改几何，缓存应该有收益；连续缩放/平移改变范围，不能期待单项缓存命中。

实验：[ui_benchmark](../../tools/ui_benchmark/main.cpp) 同数据、字体、窗口尺寸、Release配置，对缓存开关作消融对照，预热后3轮，每轮每类操作120次，交替配置顺序。分别保存总CPU路径、paintEvent、加载、峰值工作集和图元数量。

证据与解释：[UI_PERFORMANCE](../../benchmarks/reports/UI_PERFORMANCE.md)。所有配对采样图元数一致，UI测试另验证图像相同。报告包括收益不明显的缩放/平移，不只挑最好看的数字。

30秒口述：先保证结果一致，再固定条件做可重复对照，用分布而非一次paint耗时说明效果。本次测量覆盖CPU路径，没有OS输入队列和实际显示呈现；不能把倒数叫真实FPS。

## 对着代码做三个小练习

1. 把框选范围从[10,20)换成[20,30)，先手算测试里的事件数量和裁剪时长，再运行测试核对。
2. 暂时从RenderKey相等比较中移除trackIds，运行缓存过滤用例，解释为何它失败，然后恢复。
3. 打开真实样本，定位最长帧，用来源说明解释为什么本项目知道它是长间隔，却不能知道哪个Kernel导致它。

练习尚未由用户执行；不能据代码已完成推断用户已经掌握。

## 界面实测发现：帧图和时间轴横坐标不齐

截图检查发现帧图占用整个中央区域，时间轴还要给右侧滚动条留空间，两张图对同一时间的横坐标不同。首次用滚动条sizeHint补间距后，GUI几何断言仍发现6px差异。最终让两者共享QGridLayout同一列，并统一150px名称区与12px右边距，不按样式猜偏移。真实文件GUI用例校验两者宽度一致；截图另检查刻度和峰值相对位置。这是约束布局比手调像素更可靠的具体例子。
