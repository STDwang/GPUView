#include "core/trace_store.h"
#include <algorithm>
#include <limits>

namespace gpuview {
IntervalIndex::IntervalIndex(std::vector<Event> events, const CancelFlag& cancel)
    : events_(std::move(events)) {
    for (std::size_t i = 0; i < events_.size(); ++i) {
        if (i % 4096 == 0) checkCancelled(cancel);
        const auto& e = events_[i];
        if (e.start < 0 || e.duration <= 0 || e.duration > std::numeric_limits<TimeNs>::max() - e.start)
            throw std::invalid_argument("invalid interval");
    }
    std::size_t comparisons = 0;
    std::sort(events_.begin(), events_.end(), [&](const Event& a, const Event& b) {
        if (++comparisons % 4096 == 0) checkCancelled(cancel);
        return a.start < b.start || (a.start == b.start && a.id < b.id);
    });
    maxEnd_.resize(events_.size() * 4);
    if (!events_.empty()) build(1, 0, events_.size(), cancel);
}
TimeNs IntervalIndex::build(std::size_t node, std::size_t lo, std::size_t hi, const CancelFlag& cancel) {
    if (node % 4096 == 0) checkCancelled(cancel);
    if (hi - lo == 1) return maxEnd_[node] = events_[lo].end();
    const auto mid = lo + (hi - lo) / 2;
    return maxEnd_[node] = std::max(build(node * 2, lo, mid, cancel), build(node * 2 + 1, mid, hi, cancel));
}
QueryResult IntervalIndex::query(TimeRange range, std::size_t limit) const {
    QueryResult out;
    if (range.end <= range.begin || events_.empty()) return out;
    out.events.reserve(std::min(limit, events_.size()));
    visit(1, 0, events_.size(), range, limit, out);
    return out;
}
void IntervalIndex::visit(std::size_t node, std::size_t lo, std::size_t hi, TimeRange range,
                          std::size_t limit, QueryResult& out) const {
    ++out.visitedNodes;
    if (out.truncated || maxEnd_[node] <= range.begin || events_[lo].start >= range.end) return;
    if (hi - lo == 1) {
        if (out.events.size() == limit) out.truncated = true;
        else out.events.push_back(&events_[lo]);
        return;
    }
    const auto mid = lo + (hi - lo) / 2;
    visit(node * 2, lo, mid, range, limit, out);
    visit(node * 2 + 1, mid, hi, range, limit, out);
}
Snapshot buildStore(std::vector<Event> events, std::vector<std::string> trackNames,
                    std::vector<std::string> names, std::uint64_t version,
                    const CancelFlag& cancel, const Progress& progress, bool synthetic, bool frames,
                    std::string source, std::vector<std::string> warnings) {
    checkCancelled(cancel);
    if (trackNames.empty() || trackNames.size() > 1024) throw std::invalid_argument("invalid tracks");
    auto store = std::make_shared<TraceStore>();
    store->version = version;
    store->synthetic = synthetic; store->frames = frames;
    store->source = std::move(source); store->warnings = std::move(warnings);
    store->names = std::move(names);
    store->eventCount = events.size();
    std::vector<std::vector<Event>> grouped(trackNames.size());
    std::size_t seen = 0;
    for (const auto& e : events) {
        if (++seen % 4096 == 0) checkCancelled(cancel);
        if (e.track >= grouped.size() || e.name >= store->names.size() || e.start < 0 || e.duration <= 0 ||
            e.duration > std::numeric_limits<TimeNs>::max() - e.start)
            throw std::invalid_argument("event fields out of range");
        store->bounds.end = std::max(store->bounds.end, e.end());
        grouped[e.track].push_back(e);
    }
    // 释放输入副本后再建立索引，限制构建阶段的峰值内存。
    std::vector<Event>().swap(events);
    constexpr std::size_t bins = 4096;
    store->bucketWidth = store->bounds.end / TimeNs(bins) + 1;
    for (std::size_t t = 0; t < grouped.size(); ++t) {
        checkCancelled(cancel);
        Track track{trackNames[t], IntervalIndex(std::move(grouped[t]), cancel), {}, {}};
        std::vector<std::int64_t> difference(bins + 1, 0);
        seen = 0;
        if (frames) track.frameMaxDuration.resize(bins, 0);
        for (const auto& e : track.index.events()) {
            if (++seen % 4096 == 0) checkCancelled(cancel);
            const auto first = std::size_t(e.start / store->bucketWidth);
            const auto last = std::size_t((e.end() - 1) / store->bucketWidth);
            if (frames) track.frameMaxDuration[first] = std::max(track.frameMaxDuration[first], e.duration);
            ++difference[first];
            --difference[last + 1];
        }
        // 差分让超长事件覆盖很多桶时仍只写两个端点。
        std::int64_t count = 0;
        track.overviewCounts.reserve(bins);
        for (std::size_t b = 0; b < bins; ++b) {
            count += difference[b];
            track.overviewCounts.push_back(static_cast<std::uint32_t>(count));
        }
        store->tracks.push_back(std::move(track));
        if (progress) progress(50 + int(50 * (t + 1) / grouped.size()));
    }
    checkCancelled(cancel);
    return store;
}
} // namespace gpuview
