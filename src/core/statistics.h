#pragma once
#include "core/trace_store.h"
#include <optional>
namespace gpuview {
struct Statistics {
    std::size_t count = 0, longFrames = 0;
    double sumMs = 0, meanMs = 0, p50Ms = 0, p95Ms = 0, p99Ms = 0;
    std::optional<Event> longest;
    std::vector<Event> longEvents; // UI最多展示前200条，完整计数不截断。
    // 固定桶记录样本数；不是时间轴LOD桶，帧使用完整间隔，Trace使用裁剪时长。
    std::vector<std::size_t> histogram = std::vector<std::size_t>(5, 0);
};
Statistics calculateStatistics(const TraceStore& store, TimeRange range,
    const std::vector<std::uint32_t>& tracks, const CancelFlag& cancel = {});
bool isLongFrame(const std::vector<Event>& events, std::size_t index);
}
