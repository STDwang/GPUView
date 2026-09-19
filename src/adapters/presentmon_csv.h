#pragma once
#include "core/trace_store.h"
#include <filesystem>
#include <istream>
namespace gpuview {
Snapshot readPresentMon(std::istream& input, std::uint64_t version, const CancelFlag& cancel = {}, const Progress& progress = {}, const std::function<std::string()>& digest = {});
Snapshot loadPresentMon(const std::filesystem::path& path, std::uint64_t version, const CancelFlag& cancel = {}, const Progress& progress = {});
}
