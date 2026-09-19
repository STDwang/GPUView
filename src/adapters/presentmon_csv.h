/// @file src/adapters/presentmon_csv.h
/// @brief PresentMon v1 CSV输入边界：流式解析、单位校验、分组、质量记录和同次读取摘要。
#pragma once
#include "core/trace_store.h"
#include <filesystem>
#include <istream>
namespace gpuview {
/// 解析v1字段并返回只读快照；version标识会话，digest在读取结束后提供同批字节摘要，无效记录计入质量信息。
Snapshot readPresentMon(std::istream& input, std::uint64_t version, const CancelFlag& cancel = {}, const Progress& progress = {}, const std::function<std::string()>& digest = {});
/// 打开二进制CSV并在同一次读取中计算SHA-256；文件、格式错误和取消通过异常返回调用方。
Snapshot loadPresentMon(const std::filesystem::path& path, std::uint64_t version, const CancelFlag& cancel = {}, const Progress& progress = {});
}
