/// @file src/core/frame_analysis.h
/// @brief 完整帧分析数据契约及计算：行引用排序、稳定ID映射、稀疏热力图与有界概览。
#pragma once
#include "core/statistics.h"
namespace gpuview {
/// 帧表排序键；枚举值必须与表格列顺序一致。
enum class FrameSort {
    Id,        ///< 稳定事件ID。
    Start,     ///< Present时刻，纳秒。
    Duration,  ///< 完整帧间隔，纳秒。
    LongFrame  ///< 是否命中长帧判定规则。
};
/// 一个有数据的1秒时间桶，保存帧数和最大间隔；无数据秒不生成桶。
struct HeatBin {
    /// 1秒桶起点，单位会话纳秒，按Present时刻归桶。
    TimeNs begin;
    /// 该秒桶内有效帧数量，缺失桶不会作为零值样本计入。
    std::size_t count = 0;
    /// 秒桶内最大帧间隔（纳秒），max聚合保留短暂尖峰。
    TimeNs maximum = 0;
};
// 快照拥有数据，表格只保存整数索引；行排序不能改变事件ID或原始数组。
/// 不可变帧分析发布单元；共享持有原始数据，排序仅改变行引用而不移动源事件。
struct FrameAnalysis {
    /// 共享持有源只读快照，确保rows引用的事件在分析对象存活期间有效。
    Snapshot source;
    /// 本次统计/几何查询的半开纳秒范围。
    TimeRange range;
    /// 参与分析的源轨道ID集合，不能用过滤后行号代替。
    std::vector<std::uint32_t> tracks;
    /// 与当前范围和组一致的精确统计，不由热力图倒推。
    Statistics summary;
    /// 排序后的帧行引用，仅存整数位置和标记，不复制原始事件。
    std::vector<FrameRow> rows;
    /// 按ID升序保存ID到当前行号的映射，用于二分恢复选择。
    std::vector<std::pair<std::uint64_t, int>> idRows;
    /// 所选组全会话有数据的稀疏1秒桶，缺失桶不补零样本。
    std::vector<HeatBin> heat; // 稀疏1秒桶，缺失桶不补零；覆盖所选组的全会话。
    /// 后台4096格最大值概览，限制GUI绘制的扫描量。
    std::vector<TimeNs> heatOverview; // 有界4096桶最大值投影，避免绘制时扫描长会话。
    /// 当前排序字段，枚举顺序与表格四列一致。
    FrameSort sort = FrameSort::Start;
    /// 主键是否降序；同值仍以稳定ID升序确定顺序。
    bool descending = false;
    /// 按排序行索引借用源事件；越界抛异常，引用有效期依赖源快照。
    const Event& event(std::size_t row) const;
    /// 按稳定事件ID查当前排序行，不存在返回-1，避免跨排序沿用行号。
    int rowForId(std::uint64_t id) const;
};
/// 跨线程发布只读分析，持有者共同延长源快照和行索引寿命。
using FrameAnalysisPtr = std::shared_ptr<const FrameAnalysis>;
/// 后台生成单帧组的范围统计、排序行索引、ID映射和全会话热力概览；支持取消，结果只读。
FrameAnalysisPtr analyzeFrames(Snapshot source, TimeRange range, std::vector<std::uint32_t> tracks,
    FrameSort sort = FrameSort::Start, bool descending = false, const CancelFlag& cancel = {});
}
