#pragma once
#include "core/trace_store.h"
#include <optional>

namespace gpuview {
struct RenderKey {
    std::uint64_t version;
    TimeRange range;
    int width;
    std::uint32_t firstTrack;
    std::uint32_t trackCount;
    std::vector<std::uint32_t> trackIds; // 空值兼容连续轨道；UI传入过滤后的有序ID。
    bool operator==(const RenderKey& other) const;
};
struct Primitive {
    double x;
    double width;
    std::uint32_t track;
    std::uint64_t eventId;
    std::uint32_t count;
    bool aggregate;
};
struct RenderBatch {
    std::vector<Primitive> primitives;
    bool approximate = false;
    std::size_t visitedNodes = 0;
};
RenderBatch makeRenderBatch(const TraceStore& store, const RenderKey& key);

// 缓存纯几何，不缓存颜色/字体/DPR栅格；主题变更重绘即可，不需要清空几何。
class RenderCache {
public:
    const RenderBatch& get(const Snapshot& store, const RenderKey& key);
    void clear();
    std::uint64_t hits() const { return hits_; }
    std::uint64_t misses() const { return misses_; }
private:
    Snapshot owner_;
    std::optional<RenderKey> key_;
    RenderBatch batch_;
    std::uint64_t hits_ = 0;
    std::uint64_t misses_ = 0;
};
class TimeViewport {
public:
    explicit TimeViewport(TimeRange bounds = {0, 1}) { reset(bounds); }
    void reset(TimeRange bounds);
    void zoom(double factor, double anchor);
    void pan(double fraction);
    void show(TimeRange range) { if (range.end > range.begin) set(range.begin, range.end - range.begin); }
    TimeRange range() const { return range_; }
private:
    void set(TimeNs begin, TimeNs duration);
    TimeRange bounds_;
    TimeRange range_;
};
} // namespace gpuview
