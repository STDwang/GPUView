/// @file src/adapters/synthetic_source.cpp
/// @brief 固定seed教学事件发生器；为索引、渲染和测试提供可复现数据，不代表实际GPU采集。
#include "adapters/synthetic_source.h"
#include <random>
namespace gpuview {
/// 用固定seed生成count条教学事件并建索引；携带version并支持进度/取消，不能当真实GPU采集。
Snapshot generateTrace(std::size_t count, std::uint64_t version,
                       const CancelFlag& cancel, const Progress& progress) {
    if (count == 0 || count > 1000000) throw std::invalid_argument("count must be in [1,1000000]");
    std::mt19937 rng(42);
    std::vector<Event> events;
    events.reserve(count);
    constexpr TimeNs duration = 120000000000;
    std::vector<std::string> tracks;
    for (unsigned t = 0; t < 64; ++t)
        tracks.push_back(t < 8 ? "CPU / thread " + std::to_string(t) : "GPU / stream " + std::to_string(t - 8));
    for (std::size_t i = 0; i < count; ++i) {
        if (i % 4096 == 0) {
            checkCancelled(cancel);
            if (progress) progress(int(50 * i / count));
        }
        const TimeNs start = TimeNs(i) * (duration / TimeNs(count));
        const TimeNs length = i % 10007 == 0 ? 2000000000 : 1000 + rng() % 2000000;
        events.push_back({i + 1, start, length, std::uint32_t(i % 64), std::uint32_t(i % 4)});
    }
    return buildStore(std::move(events), std::move(tracks), {"Submit", "Kernel A", "Memcpy", "Kernel B"},
                      version, cancel, progress);
}
} // namespace gpuview
