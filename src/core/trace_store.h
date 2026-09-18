#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpuview {
using TimeNs = std::int64_t;
struct TimeRange {
    TimeNs begin = 0;
    TimeNs end = 1;
    bool operator==(const TimeRange& other) const { return begin == other.begin && end == other.end; }
};
struct Event {
    std::uint64_t id;
    TimeNs start;
    TimeNs duration;
    std::uint32_t track;
    std::uint32_t name;
    TimeNs end() const { return start + duration; }
};
struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("cancelled") {} };
using CancelFlag = std::shared_ptr<std::atomic_bool>;
inline void checkCancelled(const CancelFlag& flag) {
    if (flag && flag->load(std::memory_order_relaxed)) throw Cancelled();
}
using Progress = std::function<void(int)>;
struct QueryResult {
    std::vector<const Event*> events;
    bool truncated = false;
    std::size_t visitedNodes = 0;
};

// Q03：start排序并不足以查到跨越视口的长事件；子树maxEnd用于安全剪枝。
class IntervalIndex {
public:
    explicit IntervalIndex(std::vector<Event> events = {}, const CancelFlag& cancel = {});
    QueryResult query(TimeRange range, std::size_t limit = 10000) const;
    const std::vector<Event>& events() const { return events_; }
private:
    TimeNs build(std::size_t node, std::size_t lo, std::size_t hi, const CancelFlag& cancel);
    void visit(std::size_t node, std::size_t lo, std::size_t hi, TimeRange range,
               std::size_t limit, QueryResult& out) const;
    std::vector<Event> events_;
    std::vector<TimeNs> maxEnd_;
};

struct Track {
    std::string name;
    IntervalIndex index;
    // 每桶记录与其相交的事件数，不是GPU利用率，也不用于精确统计。
    std::vector<std::uint32_t> overviewCounts;
    std::vector<TimeNs> frameMaxDuration; // 帧图概览：按Present时间分桶，值为最大帧间隔。
};

// Q05：完成构建后只通过shared_ptr<const TraceStore>发布；UI没有写入口。
struct TraceStore {
    std::uint64_t version = 0;
    bool synthetic = true;
    bool frames = false;
    std::string source = "教学模拟 seed 42";
    std::vector<std::string> warnings;
    TimeRange bounds;
    TimeNs bucketWidth = 1;
    std::size_t eventCount = 0;
    std::vector<Track> tracks;
    std::vector<std::string> names;
};
using Snapshot = std::shared_ptr<const TraceStore>;
Snapshot buildStore(std::vector<Event> events, std::vector<std::string> tracks,
                    std::vector<std::string> names, std::uint64_t version,
                    const CancelFlag& cancel = {}, const Progress& progress = {},
                    bool synthetic = true, bool frames = false, std::string source = "教学模拟 seed 42",
                    std::vector<std::string> warnings = {});

// Q04：最新请求邮箱，最多一个待执行请求；丢弃的是中间任务请求，不是原始事件。
template<class T> class LatestRequest {
public:
    void replace(T value) { pending_ = std::make_unique<T>(std::move(value)); }
    std::unique_ptr<T> take() { return std::move(pending_); }
    bool hasValue() const { return bool(pending_); }
private:
    std::unique_ptr<T> pending_;
};
} // namespace gpuview
