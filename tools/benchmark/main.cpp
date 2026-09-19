/// @file tools/benchmark/main.cpp
/// @brief 查询算法基准：同批请求对照朴素扫描与索引，预热后重复测量并校验命中数。
#include "adapters/synthetic_source.h"
#include "core/render_query.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif
using namespace gpuview;
using Clock = std::chrono::steady_clock;
/// 将steady_clock起点到当前的时间差转换为毫秒，用于查询基准计时。
static double ms(Clock::time_point begin) { return std::chrono::duration<double, std::milli>(Clock::now() - begin).count(); }
/// 查询算法基准：同批请求对照朴素扫描与索引，预热后重复测量并校验命中数。
int main(int argc, char** argv) {
    std::ofstream file(argc > 1 ? argv[1] : "benchmark.csv");
    if (!file) { std::cerr << "Cannot write results\n"; return 1; }
    file << "run,events,build_ms,naive_query_ms,index_query_ms,render_batch_ms,cache_hit_ms,matches,primitives,peak_working_set_bytes\n";
    for (const std::size_t n : {100000u, 1000000u}) {
        for (int run = -1; run < 5; ++run) {
            auto begin = Clock::now(); auto store = generateTrace(n, 1); const double build = ms(begin);
            std::vector<TimeRange> queries;
            for (int q = 0; q < 100; ++q) { const TimeNs start = TimeNs(q + 1) * 1000000000; queries.push_back({start, start + 1000000}); }
            std::size_t naiveCount = 0, indexedCount = 0;
            begin = Clock::now();
            for (auto range : queries) for (const auto& track : store->tracks) for (const auto& e : track.index.events())
                if (e.start < range.end && e.end() > range.begin) ++naiveCount;
            const double naiveMs = ms(begin);
            begin = Clock::now();
            for (auto range : queries) for (const auto& track : store->tracks)
                indexedCount += track.index.query(range, n).events.size();
            const double indexedMs = ms(begin);
            if (naiveCount != indexedCount) { std::cerr << "Correctness mismatch\n"; return 2; }
            RenderCache cache; RenderKey key{1, store->bounds, 1280, 0, 16};
            begin = Clock::now(); const auto primitives = cache.get(store, key).primitives.size(); const double renderMs = ms(begin);
            begin = Clock::now();
            for (int i = 0; i < 100; ++i) cache.get(store, key);
            const double cacheMs = ms(begin) / 100;
            std::size_t peak = 0;
#ifdef _WIN32
            PROCESS_MEMORY_COUNTERS counters{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) peak = counters.PeakWorkingSetSize;
#endif
            if (run >= 0) file << run << ',' << n << ',' << build << ',' << naiveMs << ',' << indexedMs << ','
                << renderMs << ',' << cacheMs << ',' << indexedCount << ',' << primitives << ',' << peak << '\n';
        }
    }
    std::cout << "Verified 100 queries against brute force, 5 measured runs after warmup per size.\n";
}
