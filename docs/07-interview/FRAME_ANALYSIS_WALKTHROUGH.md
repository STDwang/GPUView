# 帧明细、热力图与导出：代码讲解

2026-09-19，v0.3。对应 Q01/Q02/Q04/Q05/Q06/Q10，补充已有十题，不把设计假设编造成亲历故障。

## 1. 百万行为什么不创建百万个控件？

问题：完整明细不能沿用前200条长帧快捷列表，也不能在GUI线程构造百万行QTableWidgetItem。

复现：运行 [帧基准](../../tools/frame_benchmark/main.cpp)，生成100k/1m合成帧；真实905帧只用于验证导出，不能冒充百万真实采集。

方案：[FrameAnalysis](../../src/core/frame_analysis.h)持有原始不可变快照和紧凑行索引；[FrameTableModel](../../src/ui_widgets/frame_table_model.cpp)按需提供单元格。排序请求交给 [StatisticsController](../../src/application/statistics_controller.cpp)，保留“一个运行、一个最新待办”，旧结果按代次丢弃。

```cpp
void FrameTableModel::setAnalysis(FrameAnalysisPtr analysis) {
    beginResetModel(); analysis_=std::move(analysis); endResetModel();
}
```

所有权：analysis继续持有source，行索引不会引用已释放的会话。模型只在GUI线程重置；Worker不触碰模型。不是零内存成本：完整行索引、ID映射和排序临时数据仍随帧数增长。

证据：QAbstractItemModelTester检查模型契约；百万帧后台分析中位数764.882ms、model reset中位数1.8442ms。详见 [测量报告](../../benchmarks/reports/FRAME_ANALYSIS.md)。尚未测连续排序输入延迟和屏幕FPS。

口述：我把数据规模和界面对象规模分开，后台准备不可变的排序索引，界面只为可见单元格取值，而不是创建百万个控件。

## 2. 排序后为什么选中了另一条记录？

这是用行号充当业务身份会导致的问题。复现路径：导入样本，选一条帧，再按帧间隔排序。如果只记旧行号，排序后该行可能是另一帧。

方案：[analyzeFrames](../../src/core/frame_analysis.cpp)排序后生成ID到新行号的有序表；rowForId用lower_bound查找。[FrameDetailsPanel](../../src/ui_widgets/frame_details_panel.cpp)保存稳定ID，用QSignalBlocker恢复选择，避免表格与时间轴相互触发循环。换会话清空ID，避免不同会话ID碰撞。

测试：frameModelContractAndStableSelection验证排序前后ID一致；heatmapTableAndTimelineStayInSync验证表格选择、时间轴和热力图选区。单击高亮，双击缩放定位；当前范围不含该ID时不强行选其他行。

练习：把恢复选择暂时改成旧行号，写一个两条时长不同记录的用例解释为何失败，再恢复实现。

## 3. 热力图怎样保留短暂尖峰？

问题：平均值会抹平长帧；按秒分配覆盖所有空白时间的数组又可能浪费内存。

方案：后台仅保留有数据的1秒桶，保存count和max，再投影到最多4096个概览格；[FrameHeatmap](../../src/ui_widgets/frame_heatmap.cpp)按像素聚合max。缺失为灰色N/A，不作为0ms。鼠标提示查询原始秒桶，点击按时间选1秒范围，表格与统计随之更新。

测试：frameRowsSortIdentityAndSparseHeat含稀疏时间和尖峰。公开样本每秒附近都有主动注入长帧，因此全览偏红是max口径的结果，不代表整秒每帧都慢，更不代表GPU利用率高。

边界：多个秒桶映射同一像素时不能逐桶辨认，需缩小分析范围/阅读精确提示；热力图保持全会话概览。固定60FPS预算尚不可配置。

## 4. 导出过程中切换选区、取消怎么办？

问题：逐行读取变化中的UI会导出混合选区；直接覆盖目标文件后取消会破坏旧报告。

方案：[ExportController](../../src/application/export_controller.cpp)捕获点击时的shared_ptr<const FrameAnalysis>，后台只读取此快照。100ms轮询原子进度，避免逐帧信号。析构取消并等待Worker退出。[saveAnalysis](../../src/adapters/analysis_export.cpp)用QSaveFile临时写入，关闭directWriteFallback，仅在完整写入且未取消后commit。commit是提交点，提交之后的取消不能撤销已经完成的报告。

```cpp
QSaveFile file(path);
file.setDirectWriteFallback(false);
// writeAnalysis 内分批检查取消、写入状态
writeAnalysis(file, analysis, format, notes, cancel, progress);
checkCancelled(cancel);
if (!file.commit()) throw std::runtime_error(file.errorString().toStdString());
```

示例片段省略open和异常清理，完整实现以链接代码为准。导出限制为单任务，点击期间冻结报告但仍可切换分析。当前输入路径及可解析符号链接受覆盖保护；不宣称识别所有文件系统硬链接别名。

证据：exportCancelMidWritePreservesOriginal在3000条数据写入中途取消，旧报告仍完整；exportControllerUsesFrozenSnapshotAndProtectsInput验证快照冻结和输入保护；独立PowerShell CSV解析器验证905条不截断、排序、唯一ID、引号和换行。写入失败测试覆盖无效目标目录，未模拟磁盘突然断电。

## 5. 怎样证明报告对应哪个输入？

[PresentMon读取器](../../src/adapters/presentmon_csv.cpp)对本次解析实际消费的文件字节同步计算SHA-256，避免另读一次文件产生竞态。报告带应用/格式版本、组、范围、统计/长帧规则、输入有效性计数、警告、备注及限制；不导出完整本机路径。

测试：importedBytesHaveMatchingHashAndQuality；公开样本外部Get-FileHash与导出摘要一致。SHA针对原始字节，换行改变会改变摘要；摘要证明输入字节一致，不证明采集真实性或GPU因果关系。

练习：先给样本增加一行换行再导入，观察摘要变化；对比选区导出和全会话导出的count，解释为什么图上降采样不影响统计或明细数量。
