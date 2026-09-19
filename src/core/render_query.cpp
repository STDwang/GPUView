/// @file src/core/render_query.cpp
/// @brief 可见几何查询、LOD、单项几何缓存和时间视口；不依赖Qt，方便独立验证。
#include "core/render_query.h"
#include <algorithm>
#include <cmath>

namespace gpuview {
/// 比较组成状态的全部字段，用于时间范围一致性或完整缓存键判等。
bool RenderKey::operator==(const RenderKey& other) const {
    return version == other.version && range == other.range && width == other.width &&
           firstTrack == other.firstTrack && trackCount == other.trackCount && trackIds == other.trackIds;
}
/// 根据时间/轨道/宽度生成几何；密集数据用有界概览，近似图元不用于精确统计。
RenderBatch makeRenderBatch(const TraceStore& store, const RenderKey& key) {
    RenderBatch out;
    if (key.width <= 0 || key.range.end <= key.range.begin) return out;
    const double scale = key.width / double(key.range.end - key.range.begin);
    const auto total = key.trackIds.empty() ? store.tracks.size() : key.trackIds.size();
    const auto endTrack = std::min(total, std::size_t(key.firstTrack) + key.trackCount);
    for (std::size_t visible = key.firstTrack; visible < endTrack; ++visible) {
        const auto t = key.trackIds.empty() ? std::uint32_t(visible) : key.trackIds[visible];
        if (t >= store.tracks.size()) continue;
        const auto& track = store.tracks[t];
        // 查询设置硬上限，密集视口切到预计算概览，避免GUI扫描全部事件。
        const auto exact = track.index.query(key.range, std::size_t(key.width) * 2);
        out.visitedNodes += exact.visitedNodes;
        if (!exact.truncated) {
            for (auto* e : exact.events) {
                const auto begin = std::max(e->start, key.range.begin);
                const auto end = std::min(e->end(), key.range.end);
                out.primitives.push_back({double(begin - key.range.begin) * scale,
                    std::max(1.0, double(end - begin) * scale), std::uint32_t(t), e->id, 1, false});
            }
        } else {
            out.approximate = true;
            std::vector<std::uint32_t> pixels(std::size_t(key.width), 0);
            // 概览桶是保守的粗略显示；精确拾取仍回到原始区间索引。
            for (std::size_t b = 0; b < track.overviewCounts.size(); ++b) {
                const TimeNs begin = TimeNs(b) * store.bucketWidth;
                const TimeNs end = begin + std::min(store.bounds.end - begin, store.bucketWidth);
                if (end <= key.range.begin || begin >= key.range.end || !track.overviewCounts[b]) continue;
                const int first = std::clamp(int(double(std::max(begin, key.range.begin) - key.range.begin) * scale), 0, key.width - 1);
                const int last = std::clamp(int(std::ceil(double(std::min(end, key.range.end) - key.range.begin) * scale)) - 1, first, key.width - 1);
                for (int x = first; x <= last; ++x)
                    pixels[std::size_t(x)] = std::max(pixels[std::size_t(x)], track.overviewCounts[b]);
            }
            for (int x = 0; x < key.width; ++x)
                if (pixels[std::size_t(x)]) out.primitives.push_back({double(x), 1.0, std::uint32_t(t), 0, pixels[std::size_t(x)], true});
        }
    }
    return out;
}
/// 同快照同键复用几何，否则重建；返回引用在下一次get或clear后可能失效。
const RenderBatch& RenderCache::get(const Snapshot& store, const RenderKey& key) {
    if (!store) throw std::invalid_argument("snapshot required");
    if (owner_ == store && key_ && *key_ == key) { ++hits_; return batch_; }
    batch_ = makeRenderBatch(*store, key);
    owner_ = store;
    key_ = key;
    ++misses_;
    return batch_;
}
/// 释放缓存快照、键和几何；保留累计命中计数供诊断。
void RenderCache::clear() { owner_.reset(); key_.reset(); batch_ = {}; }
/// 重设总边界并恢复全览；负起点、空或反向范围抛invalid_argument。
void TimeViewport::reset(TimeRange bounds) {
    if (bounds.begin < 0 || bounds.end <= bounds.begin) throw std::invalid_argument("invalid bounds");
    bounds_ = range_ = bounds;
}
/// 统一夹紧起点与时长，避免缩放和平移越过全会话边界。
void TimeViewport::set(TimeNs begin, TimeNs duration) {
    duration = std::clamp(duration, TimeNs(1), bounds_.end - bounds_.begin);
    begin = std::clamp(begin, bounds_.begin, bounds_.end - duration);
    range_ = {begin, begin + duration};
}
/// 按factor缩放时长，尽量保持相对位置anchor所指时刻不变；非法参数忽略。
void TimeViewport::zoom(double factor, double anchor) {
    if (!std::isfinite(factor) || factor <= 0 || !std::isfinite(anchor)) return;
    anchor = std::clamp(anchor, 0.0, 1.0);
    const auto old = range_.end - range_.begin;
    const auto length = TimeNs(std::clamp(double(old) / factor, 1.0, double(bounds_.end - bounds_.begin)));
    set(range_.begin + TimeNs(double(old - length) * anchor), length);
}
/// 按当前可见时长的fraction平移，最终范围夹紧到数据边界。
void TimeViewport::pan(double fraction) {
    if (!std::isfinite(fraction)) return;
    const auto length = range_.end - range_.begin;
    const auto shift = TimeNs(std::clamp(fraction, -1.0, 1.0) * double(length));
    // 数据域非负；先夹紧余量以防int64溢出。
    const auto move = std::clamp(shift, bounds_.begin - range_.begin, bounds_.end - range_.end);
    set(range_.begin + move, length);
}
} // namespace gpuview
