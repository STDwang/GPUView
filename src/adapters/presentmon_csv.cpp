#include "adapters/presentmon_csv.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <tuple>
namespace gpuview {
namespace {
// 逐记录读取，带引号字段可跨行；长记录限制和取消点防止损坏文件拖住退出。
bool record(std::istream& in, std::vector<std::string>& fields, const CancelFlag& cancel) {
    fields.clear(); std::string field; bool quoted = false, closed = false, any = false; char c; std::size_t bytes = 0;
    while (in.get(c)) {
        if (++bytes % 4096 == 0) checkCancelled(cancel);
        if (bytes > 1024 * 1024) throw std::runtime_error("CSV record exceeds 1 MiB");
        any = true;
        if (quoted) {
            if (c == '"') { if (in.peek() == '"') { in.get(c); field += c; } else { quoted = false; closed = true; } }
            else field += c;
        } else if (c == ',') { fields.push_back(field); field.clear(); closed = false; }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && in.peek() == '\n') in.get(c);
            fields.push_back(field); return true;
        } else if (c == '"' && field.empty() && !closed) quoted = true;
        else { if (closed || c == '"') throw std::runtime_error("Malformed CSV quoting"); field += c; }
    }
    if (quoted) throw std::runtime_error("Unterminated CSV field");
    if (any) fields.push_back(field);
    return any;
}
std::string lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}
TimeNs number(const std::string& s, long double scale) {
    std::istringstream in(s); in.imbue(std::locale::classic()); long double v;
    if (!(in >> v) || !(in >> std::ws).eof() || !std::isfinite(v) || v < 0 || v * scale >= 9.0e18L)
        throw std::runtime_error("invalid number");
    return TimeNs(std::llround(v * scale));
}
}
Snapshot readPresentMon(std::istream& input, std::uint64_t version, const CancelFlag& cancel, const Progress& progress) {
    checkCancelled(cancel);
    if (input.peek() == 0xef) { char bom[3]{}; input.read(bom, 3); if (std::string(bom, 3) != "\xef\xbb\xbf") throw std::runtime_error("Invalid BOM"); }
    std::vector<std::string> fields;
    if (!record(input, fields, cancel)) throw std::runtime_error("Empty CSV");
    std::map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < fields.size(); ++i)
        if (!columns.emplace(lower(fields[i]), i).second) throw std::runtime_error("Duplicate CSV header");
    const auto columnCount = fields.size();
    auto required = [&](const char* name) {
        auto it = columns.find(name); if (it == columns.end()) throw std::runtime_error(std::string("Unsupported PresentMon v1 schema, missing: ") + name);
        return it->second;
    };
    const auto app = required("application"), pid = required("processid"), chain = required("swapchainaddress");
    const auto timestamp = required("timeinseconds"), duration = required("msbetweenpresents");
    std::map<std::tuple<std::string,std::string,std::string>, std::uint32_t> trackIds;
    std::vector<std::string> tracks, warnings;
    std::vector<Event> events;
    std::size_t row = 1, rejected = 0, duplicates = 0, unordered = 0;
    std::map<std::uint32_t, TimeNs> previous;
    TimeNs origin = std::numeric_limits<TimeNs>::max();
    while (record(input, fields, cancel)) {
        checkCancelled(cancel); ++row;
        if (fields.size() == 1 && fields[0].empty()) continue;
        try {
            if (fields.size() != columnCount) throw std::runtime_error("column count");
            const auto t = number(fields[timestamp], 1e9L), d = number(fields[duration], 1e6L);
            if (d <= 0 || d > std::numeric_limits<TimeNs>::max() - t) throw std::runtime_error("duration");
            if (fields[app].empty() || fields[pid].empty() || fields[chain].empty()) throw std::runtime_error("identity");
            const auto key = fields[app] + " / PID " + fields[pid] + " / " + fields[chain];
            auto inserted = trackIds.emplace(std::make_tuple(fields[app], fields[pid], fields[chain]), std::uint32_t(tracks.size()));
            if (inserted.second) tracks.push_back(key);
            const auto track = inserted.first->second;
            if (previous.count(track)) { duplicates += t == previous[track]; unordered += t < previous[track]; }
            previous[track] = t; origin = std::min(origin, t);
            events.push_back({row, t, d, track, 0});
        } catch (const std::runtime_error&) {
            ++rejected;
            if (warnings.size() < 12) warnings.push_back("无效记录 #" + std::to_string(row) + "（未补零、未跨行推导间隔）");
        }
    }
    if (events.empty()) throw std::runtime_error("CSV contains no valid frame intervals");
    for (auto& e : events) e.start -= origin;
    warnings.push_back("有效 " + std::to_string(events.size()) + " / 排除 " + std::to_string(rejected) +
        " / 相邻重复时间 " + std::to_string(duplicates) + " / 无序记录 " + std::to_string(unordered));
    warnings.push_back("全部有效应用Present间隔，未筛选Dropped。矩形锚定当前Present，宽度编码前一间隔，不是GPU执行区间。");
    if (progress) progress(50);
    return buildStore(std::move(events), std::move(tracks), {"Present间隔"}, version, cancel, progress,
        false, true, "PresentMon v1字段 / TimeInSeconds + MsBetweenPresents / 原点(ns)=" + std::to_string(origin), std::move(warnings));
}
Snapshot loadPresentMon(const std::filesystem::path& path, std::uint64_t version, const CancelFlag& cancel, const Progress& progress) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open PresentMon CSV");
    return readPresentMon(input, version, cancel, progress);
}
}
