# 带注释源码阅读入口

2026-09-19：本轮为41个自有C++头文件/实现文件，以及3个PowerShell脚本和2个CMake文件补齐中文说明。第三方SDK、构建生成文件不手工维护注释。

## 注释怎么读

- 文件顶部说明模块职责及数据边界。
- 类/结构体说明角色；每个显式成员字段说明语义、单位或所有权。
- 每个显式函数都说明职责与关键约束，包含构造/析构、Qt信号、事件处理、内联访问器、测试入口；局部回调另解释捕获、线程或联动目的。
- 注释描述当前实现，不能代替测试，也不代表新增功能或性能提升。三斜线注释可由编辑器识别；当前未新增Doxygen生成流程。

## 建议按一条数据流阅读

| 顺序 | 文件 | 先回答的问题 |
|---|---|---|
| 1 | [trace_store.h](../../src/core/trace_store.h) | Event的start/duration代表什么？Snapshot为什么只读？指针能活多久？ |
| 2 | [presentmon_csv.cpp](../../src/adapters/presentmon_csv.cpp) | 输入在哪校验？何处从秒/毫秒转换纳秒？摘要为什么在同次读取中计算？ |
| 3 | [session_controller.cpp](../../src/application/session_controller.cpp) | Worker捕获谁？谁更新UI？取消后为何还能出现完成回调？ |
| 4 | [render_query.cpp](../../src/core/render_query.cpp) | 精确查询何时转为LOD？缓存键为什么包含过滤后的轨道ID？ |
| 5 | [timeline_widget.h](../../src/ui_widgets/timeline_widget.h) | 哪些字段是源数据，哪些是交互状态？firstTrack_为什么不是源轨道ID？ |
| 6 | [frame_analysis.cpp](../../src/core/frame_analysis.cpp) | 表格排序移动了什么？ID到行号怎样恢复？热力图为什么取max？ |
| 7 | [frame_table_model.cpp](../../src/ui_widgets/frame_table_model.cpp) | data为何按需取值？为什么不在sort里直接排序？ |
| 8 | [export_controller.cpp](../../src/application/export_controller.cpp)、[analysis_export.cpp](../../src/adapters/analysis_export.cpp) | 如何冻结报告、取消写入、保留旧文件？原子提交的边界在哪里？ |
| 9 | [core_tests.cpp](../../tests/core/core_tests.cpp)、[ui_tests.cpp](../../tests/ui/ui_tests.cpp) | 哪个测试能证明上述解释？它又没有证明什么？ |

## 三个口述练习

1. 沿SessionController的成员注释解释worker_、cancel_、pending_、generation_、current_的差别；结合Q04/Q05/Q06讲一次“加载中取消又重新加载”。
2. 沿RenderKey与TimelineWidget解释range、firstTrack、trackIds如何共同影响缓存；结合Q01/Q03/Q08说明漏一个键字段可能出现的错误。
3. 沿FrameAnalysis和FrameTableModel解释source、rows、idRows和analysis_的所有权；结合Q02说明百万行不等于百万控件，再用实际基准回答Q10。

不要只背注释。对每题找到对应测试，先预测结果，再运行或单步观察。算法、导出和性能的完整案例仍见[帧分析讲解](FRAME_ANALYSIS_WALKTHROUGH.md)与[十题清单](QUESTIONS.md)。

## 本轮验证边界

对41个C++文件去除注释并比较代码，除三组逗号并列成员声明拆成逐项声明外，代码保持一致；成员顺序、类型、默认值未改。3个PowerShell脚本通过语法解析，去掉注释后的执行token保持一致。Debug构建和52个实际用例通过（Core 42、UI 10；Qt总数44+12含初始化/清理）。

未新增重复测试，未重跑性能基准，也未据注释更新推断性能改善。既有Release结果仍属于v0.3功能交付记录。
