/// @file src/core/statistics.cpp
/// @brief 精确范围统计与长帧判断；区分Trace裁剪时长和Present完整间隔，不使用LOD近似数据。
#include "core/statistics.h"
#include <algorithm>
#include <cmath>
namespace gpuview {
/// 按60FPS预算及此前最多120条历史间隔判定长帧；历史不足30条只用预算，调用方保证index有效。
bool isLongFrame(const std::vector<Event>& events, std::size_t index) {
    double threshold = 2.0 * 1000000000.0 / 60.0;
    const auto count = std::min<std::size_t>(120, index);
    if (count >= 30) {
        std::vector<TimeNs> previous;
        for (auto i = index - count; i < index; ++i) previous.push_back(events[i].duration);
        std::sort(previous.begin(), previous.end());
        const double median = count % 2 ? double(previous[count / 2]) :
            (double(previous[count / 2 - 1]) + double(previous[count / 2])) / 2;
        threshold = std::max(threshold, median * 2);
    }
    return double(events[index].duration) > threshold;
}
/// 统计tracks中的range：帧按Present归属并保留完整间隔，Trace按相交时长裁剪；可选收集完整帧行索引，支持取消。
Statistics calculateStatistics(const TraceStore& store, TimeRange range,
    const std::vector<std::uint32_t>& tracks, const CancelFlag& cancel, std::vector<FrameRow>* frameRows) {
    checkCancelled(cancel);
    Statistics out;
    if (frameRows) frameRows->clear();
    if (range.end <= range.begin) return out;
    std::vector<TimeNs> durations;
    TimeNs longest = -1;
    for (auto track : tracks) {
        if (track >= store.tracks.size()) continue;
        const auto& events = store.tracks[track].index.events();
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (i % 1024 == 0) checkCancelled(cancel);
            const auto& e = events[i];
            if (e.start >= range.end) break;
            if (store.frames ? e.start < range.begin : e.end() <= range.begin) continue;
            const auto duration = store.frames ? e.duration : std::min(e.end(), range.end) - std::max(e.start, range.begin);
            durations.push_back(duration);
            out.sumMs += double(duration) / 1e6;
            if (duration > longest) { longest = duration; out.longest = e; }
            const double ms = double(duration) / 1e6;
            ++out.histogram[ms <= 8.33 ? 0 : ms <= 16.67 ? 1 : ms <= 33.33 ? 2 : ms <= 50 ? 3 : 4];
            if (store.frames) {
                const bool longFrame = isLongFrame(events, i);
                if (longFrame) { ++out.longFrames; if (out.longEvents.size() < 200) out.longEvents.push_back(e); }
                if (frameRows) frameRows->push_back({track, i, longFrame});
            }
        }
    }
    std::size_t comparisons = 0;
    // 为nearest-rank分位数排序原始时长，比较器分批检查取消。
    std::sort(durations.begin(), durations.end(), [&](auto a, auto b) {
        if (++comparisons % 4096 == 0) checkCancelled(cancel);
        return a < b;
    });
    checkCancelled(cancel);
    out.count = durations.size();
    if (out.count) {
        out.meanMs = out.sumMs / out.count;
        // 以ceil(p*N)-1取得nearest-rank样本；仅非空样本调用，最后转换为毫秒。
        auto percentile = [&](double p) { return double(durations[std::size_t(std::ceil(p * out.count)) - 1]) / 1e6; };
        out.p50Ms = percentile(.5); out.p95Ms = percentile(.95); out.p99Ms = percentile(.99);
    }
    return out;
}
}
