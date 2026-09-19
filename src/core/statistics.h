/// @file src/core/statistics.h
/// @brief 精确范围统计与长帧判断；区分Trace裁剪时长和Present完整间隔，不使用LOD近似数据。
#pragma once
#include "core/trace_store.h"
#include <optional>
namespace gpuview {
/// 一条表格行的轻量引用：源轨道、事件索引及长帧标记，不复制事件。
struct FrameRow {
    /// 源轨道数组的索引，不是过滤后的可见行号。
    std::uint32_t track;
    /// 帧在原始轨道有序事件数组中的位置，依赖源快照存活。
    std::size_t index;
    /// 按固定预算和完整历史基线判断的长帧标记。
    bool longFrame;
};
/// 一次范围分析的精确结果；均值和百分位单位毫秒，空结果由count区分。
struct Statistics {
    /// 纳入当前结果的样本/事件数，不是GPU利用率。
    std::size_t count = 0;
    /// 范围内完整长帧计数，不受快捷列表200条限制。
    std::size_t longFrames = 0;
    /// 纳入样本的总时长（毫秒）；并发Trace求和可超过墙钟时间。
    double sumMs = 0;
    /// 平均时长（毫秒）；空结果内部为0，展示层应标N/A。
    double meanMs = 0;
    /// nearest-rank第50百分位时长（毫秒）。
    double p50Ms = 0;
    /// nearest-rank第95百分位时长（毫秒）。
    double p95Ms = 0;
    /// nearest-rank第99百分位时长（毫秒）。
    double p99Ms = 0;
    /// 按统计口径选出的最长项原始事件副本，无样本则为空。
    std::optional<Event> longest;
    /// 前200条长帧快捷项；完整数量看longFrames，完整明细另有行索引。
    std::vector<Event> longEvents; // UI最多展示前200条，完整计数不截断。
    // 固定桶记录样本数；不是时间轴LOD桶，帧使用完整间隔，Trace使用裁剪时长。
    /// 五档时长分布数量，帧用完整间隔，Trace用裁剪时长，不是LOD时间桶。
    std::vector<std::size_t> histogram = std::vector<std::size_t>(5, 0);
};
/// 统计tracks中的range：帧按Present归属并保留完整间隔，Trace按相交时长裁剪；可选收集完整帧行索引，支持取消。
Statistics calculateStatistics(const TraceStore& store, TimeRange range,
    const std::vector<std::uint32_t>& tracks, const CancelFlag& cancel = {}, std::vector<FrameRow>* frameRows = nullptr);
/// 按60FPS预算及此前最多120条历史间隔判定长帧；历史不足30条只用预算，调用方保证index有效。
bool isLongFrame(const std::vector<Event>& events, std::size_t index);
}
