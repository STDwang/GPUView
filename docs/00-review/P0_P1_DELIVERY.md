# P0/P1改进交付清单

本清单对应2026-09-18用户确认继续实施的改进项，区别于完整PRD中尚未完成的A/B任务。代码采用C++17/Qt6 Widgets，核心算法无Qt依赖。

| 级别 | 项目 | 当前交付 | 证据入口 |
|---|---|---|---|
| P0 | 事件选中反馈 | 金色事件边框、轨道底色、起止/时长/来源详情；Esc清除 | TimelineWidget；eventSelectionAndZoomHistory |
| P0 | 框选统计 | 后台精确数量/总时长/均值/P50/P95/P99/分布；最长项跳转；原始数据口径 | statistics.cpp；手算与半开边界用例 |
| P0 | 真实帧闭环 | 实采905条Present记录；CSV导入、进程/交换链分组、帧曲线、长帧表和双击定位 | data/samples；realCaptureFixture；realFileAnalysisAndLongFrameNavigation |
| P0 | 性能验证 | 100k/1m加载、进程峰值工作集、三类连续交互P50/P95；缓存开关对照及图像一致性 | UI_PERFORMANCE.md和原始CSV |
| P1 | 轨道导航 | 名称过滤、勾选显示/隐藏、CPU/GPU组折叠；名称区滚轮滚动 | navigationFilterAndGroupCollapse |
| P1 | 时间轴可读性 | 自动单位/小数位、悬停十字时间线和时间提示、选区缩放、64项视图历史 | TimelineWidget；视图历史回归 |
| P1 | 密集事件说明 | 全览标注LOD/近似；亮度规则图例、悬停桶计数；精确选择与统计分离 | TimelineWidget；boundedDenseRendering |
| P1 | 侧栏和状态 | 事件/统计/学习/来源独立页；空会话、加载、取消、失败保留数据；视图菜单恢复侧栏 | MainWindow；fileLoadFailurePreservesSnapshot |

## 实现边界

- 真实样本来自受控D3D11负载，主动制造长帧。它验证真实ETW→CSV→分析链路，不证明已定位真实游戏GPU瓶颈。
- CSV目前只支持经过本机验证的v1字段，拒绝未知时间模式。按全部有效Present间隔统计，暂无Dropped筛选、GPU可选字段和目标FPS选择。固定60FPS预算的长帧规则只是筛查。
- 完整帧明细Model/View、帧热力图、文件导出、遥测、多源对齐、Trace文件、回放、干净部署与Quick属于后续；本轮不把它们写成已完成。
- UI基准是离屏CPU路径，不能证明物理屏幕输入延迟或稳定刷新率。QPainter/QGraphicsView/Quick的横向实测尚未进行。
- 学习材料已提供，用户是否能口述/修改尚未验收。

[运行说明](../../README.md) · [面试讲解](../07-interview/P0_P1_WALKTHROUGH.md) · [性能报告](../../benchmarks/reports/UI_PERFORMANCE.md)
