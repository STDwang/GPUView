#pragma once
#include "core/statistics.h"
namespace gpuview {
enum class FrameSort { Id, Start, Duration, LongFrame };
struct HeatBin { TimeNs begin; std::size_t count = 0; TimeNs maximum = 0; };
// 快照拥有数据，表格只保存整数索引；行排序不能改变事件ID或原始数组。
struct FrameAnalysis {
    Snapshot source;
    TimeRange range;
    std::vector<std::uint32_t> tracks;
    Statistics summary;
    std::vector<FrameRow> rows;
    std::vector<std::pair<std::uint64_t, int>> idRows;
    std::vector<HeatBin> heat; // 稀疏1秒桶，缺失桶不补零；覆盖所选组的全会话。
    std::vector<TimeNs> heatOverview; // 有界4096桶最大值投影，避免绘制时扫描长会话。
    FrameSort sort = FrameSort::Start;
    bool descending = false;
    const Event& event(std::size_t row) const;
    int rowForId(std::uint64_t id) const;
};
using FrameAnalysisPtr = std::shared_ptr<const FrameAnalysis>;
FrameAnalysisPtr analyzeFrames(Snapshot source, TimeRange range, std::vector<std::uint32_t> tracks,
    FrameSort sort = FrameSort::Start, bool descending = false, const CancelFlag& cancel = {});
}
