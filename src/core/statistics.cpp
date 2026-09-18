#include "core/statistics.h"
#include <algorithm>
#include <cmath>
namespace gpuview {
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
Statistics calculateStatistics(const TraceStore& store, TimeRange range,
    const std::vector<std::uint32_t>& tracks, const CancelFlag& cancel) {
    checkCancelled(cancel);
    Statistics out;
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
            if (store.frames && isLongFrame(events, i)) { ++out.longFrames; if (out.longEvents.size() < 200) out.longEvents.push_back(e); }
        }
    }
    std::size_t comparisons = 0;
    std::sort(durations.begin(), durations.end(), [&](auto a, auto b) {
        if (++comparisons % 4096 == 0) checkCancelled(cancel);
        return a < b;
    });
    checkCancelled(cancel);
    out.count = durations.size();
    if (out.count) {
        out.meanMs = out.sumMs / out.count;
        auto percentile = [&](double p) { return double(durations[std::size_t(std::ceil(p * out.count)) - 1]) / 1e6; };
        out.p50Ms = percentile(.5); out.p95Ms = percentile(.95); out.p99Ms = percentile(.99);
    }
    return out;
}
}
