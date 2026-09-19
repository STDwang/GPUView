/// @file src/adapters/presentmon_csv.cpp
/// @brief PresentMon v1 CSV输入边界：流式解析、单位校验、分组、质量记录和同次读取摘要。
#include "adapters/presentmon_csv.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <tuple>
#include <QCryptographicHash>
namespace gpuview {
namespace {
// 对同一次解析所读取的字节做哈希，避免先哈希再打开导致文件变更时摘要与内容不一致。
/// 包装输入流的8192字节缓冲，在被解析的同次读取中计算SHA-256。
class HashingBuffer : public std::streambuf {
public:
    /// 借用源流和摘要累加器，两者须比缓冲活得更久；共享取消标志控制读取退出。
    HashingBuffer(std::istream& source, QCryptographicHash& hash, CancelFlag cancel)
        : source_(source), hash_(hash), cancel_(std::move(cancel)) {}
protected:
    /// 缓冲耗尽时读取一批字节并更新摘要，支持取消并区分正常EOF与读取失败。
    int_type underflow() override {
        if(gptr() && gptr()<egptr()) return traits_type::to_int_type(*gptr());
        checkCancelled(cancel_); source_.read(buffer_,sizeof(buffer_)); const auto count=source_.gcount();
        if(source_.bad()) throw std::runtime_error("CSV read failed");
        if(!count) return traits_type::eof();
        hash_.addData(QByteArrayView(buffer_,qsizetype(count)));
        setg(buffer_,buffer_,buffer_+count); return traits_type::to_int_type(*gptr());
    }
private:
    /// 借用底层输入流，HashingBuffer不拥有也不关闭该流。
    std::istream& source_;
    /// 借用同次读取SHA-256累加器，寿命须覆盖缓冲读取过程。
    QCryptographicHash& hash_;
    /// 共享原子取消标志，仅请求协作退出，不强制终止线程。
    CancelFlag cancel_;
    /// 8192字节读取缓冲，作为streambuf的get区域并分批计算摘要。
    char buffer_[8192];
};
// 逐记录读取，带引号字段可跨行；长记录限制和取消点防止损坏文件拖住退出。
/// 读取支持引号、双引号及换行的CSV记录到fields；EOF返回false，格式损坏抛异常。
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
/// 将列名转小写以便不区分大小写匹配，不修改原始字段内容。
std::string lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}
/// 按C locale解析非负有限数，乘scale转纳秒；非法字符、负数或溢出拒绝。
TimeNs number(const std::string& s, long double scale) {
    std::istringstream in(s); in.imbue(std::locale::classic()); long double v;
    if (!(in >> v) || !(in >> std::ws).eof() || !std::isfinite(v) || v < 0 || v * scale >= 9.0e18L)
        throw std::runtime_error("invalid number");
    return TimeNs(std::llround(v * scale));
}
}
/// 解析v1字段并返回只读快照；version标识会话，digest在读取结束后提供同批字节摘要，无效记录计入质量信息。
Snapshot readPresentMon(std::istream& input, std::uint64_t version, const CancelFlag& cancel, const Progress& progress, const std::function<std::string()>& digest) {
    checkCancelled(cancel);
    if (input.peek() == 0xef) { char bom[3]{}; input.read(bom, 3); if (std::string(bom, 3) != "\xef\xbb\xbf") throw std::runtime_error("Invalid BOM"); }
    std::vector<std::string> fields;
    if (!record(input, fields, cancel)) throw std::runtime_error("Empty CSV");
    std::map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < fields.size(); ++i)
        if (!columns.emplace(lower(fields[i]), i).second) throw std::runtime_error("Duplicate CSV header");
    const auto columnCount = fields.size();
    // 按标准化列名查必要字段位置，缺失立即报错，不猜测其他版本的字段含义。
    auto required = [&](const char* name) {
        auto it = columns.find(name); if (it == columns.end()) throw std::runtime_error(std::string("Unsupported PresentMon v1 schema, missing: ") + name);
        return it->second;
    };
    const auto app = required("application"), pid = required("processid"), chain = required("swapchainaddress");
    const auto timestamp = required("timeinseconds"), duration = required("msbetweenpresents");
    /// 过滤后的有序源轨道ID；为空时兼容连续轨道模式。
    std::map<std::tuple<std::string,std::string,std::string>, std::uint32_t> trackIds;
    /// 解析或质量限制说明，供来源页及导出共同使用。
    std::vector<std::string> tracks, warnings;
    /// 命中的非拥有事件指针，使用期间须保持对应索引/快照存活。
    std::vector<Event> events;
    /// 轨道行高（逻辑像素），用于裁剪、滚动和鼠标命中。
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
            /// 源轨道数组的索引，不是过滤后的可见行号。
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
    SourceInfo sourceInfo{digest ? digest() : std::string(), events.size()+rejected, rejected};
    if (progress) progress(50);
    return buildStore(std::move(events), std::move(tracks), {"Present间隔"}, version, cancel, progress,
        false, true, "PresentMon v1字段 / TimeInSeconds + MsBetweenPresents / 原点(ns)=" + std::to_string(origin), std::move(warnings), std::move(sourceInfo));
}
/// 打开二进制CSV并在同一次读取中计算SHA-256；文件、格式错误和取消通过异常返回调用方。
Snapshot loadPresentMon(const std::filesystem::path& path, std::uint64_t version, const CancelFlag& cancel, const Progress& progress) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open PresentMon CSV");
    QCryptographicHash hash(QCryptographicHash::Sha256); HashingBuffer buffer(input,hash,cancel);
    std::istream parsed(&buffer); parsed.exceptions(std::ios::badbit);
    // 摘要回调只在解析读取结束后取最终哈希，避免再次打开文件造成内容与摘要不一致。
    return readPresentMon(parsed, version, cancel, progress, [&hash] { return hash.result().toHex().toStdString(); });
}
}
