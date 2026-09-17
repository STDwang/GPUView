#pragma once
#include "core/trace_store.h"
namespace gpuview {
Snapshot generateTrace(std::size_t count, std::uint64_t version,
                       const CancelFlag& cancel = {}, const Progress& progress = {});
}
