/// @file src/adapters/synthetic_source.h
/// @brief 固定seed教学事件发生器；为索引、渲染和测试提供可复现数据，不代表实际GPU采集。
#pragma once
#include "core/trace_store.h"
namespace gpuview {
/// 用固定seed生成count条教学事件并建索引；携带version并支持进度/取消，不能当真实GPU采集。
Snapshot generateTrace(std::size_t count, std::uint64_t version,
                       const CancelFlag& cancel = {}, const Progress& progress = {});
}
