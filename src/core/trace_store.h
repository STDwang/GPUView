/// @file src/core/trace_store.h
/// @brief 纳秒数据契约、只读快照、区间索引与有界待办；核心层不依赖Qt或界面线程。
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpuview {
/// 核心层统一用有符号64位纳秒；仅在显示/导出边界转换毫秒。
using TimeNs = std::int64_t;
/// 会话纳秒半开区间[begin,end)，边界语义在查询、选择和统计中保持一致。
struct TimeRange {
    /// 半开区间起点，单位会话纳秒，包含该时刻。
    TimeNs begin = 0;
    /// 半开区间终点，单位会话纳秒，不包含该时刻。
    TimeNs end = 1;
    /// 比较组成状态的全部字段，用于时间范围一致性或完整缓存键判等。
    bool operator==(const TimeRange& other) const { return begin == other.begin && end == other.end; }
};
/// 紧凑原始事件；Trace表达执行区间，帧数据以Present时刻锚定前一间隔。
struct Event {
    /// 会话内稳定事件ID；排序与过滤不改变身份。
    std::uint64_t id;
    /// 事件起点或帧Present时刻，单位会话纳秒。
    TimeNs start;
    /// 事件时长或前一Present间隔（纳秒）；帧间隔不等于GPU执行时间。
    TimeNs duration;
    /// 源轨道数组的索引，不是过滤后的可见行号。
    std::uint32_t track;
    /// 事件名称字典索引，避免每条记录重复保存字符串。
    std::uint32_t name;
    /// 返回纳秒终点；建立索引时已校验正时长及加法不溢出。
    TimeNs end() const { return start + duration; }
};
/// 协作取消专用异常类型，与输入损坏或运行失败分开处理。
struct Cancelled : std::runtime_error {
    /// 构造可与输入错误区分的取消异常，由控制器转为取消状态。
    Cancelled() : std::runtime_error("cancelled") {}
};
/// 任务和协调器共享的取消令牌；relaxed用于通知，不承担结果发布同步。
using CancelFlag = std::shared_ptr<std::atomic_bool>;
/// 读共享原子标志；置位抛Cancelled，空标志表示未启用取消。
inline void checkCancelled(const CancelFlag& flag) {
    if (flag && flag->load(std::memory_order_relaxed)) throw Cancelled();
}
/// 0到100的进度回调，运行在生产者线程；接收方应写邮箱而非直接操作UI。
using Progress = std::function<void(int)>;
/// 有限容量查询结果，显式报告截断，事件指针不拥有源存储。
struct QueryResult {
    /// 命中的非拥有事件指针，使用期间须保持对应索引/快照存活。
    std::vector<const Event*> events;
    /// 因limit而省略额外命中时为true，不能把截断查询当全量统计。
    bool truncated = false;
    /// 查询访问的树节点数量，用于算法成本诊断。
    std::size_t visitedNodes = 0;
};

// Q03：start排序并不足以查到跨越视口的长事件；子树maxEnd用于安全剪枝。
/// 按start排序并以子树maxEnd增强的区间索引，能查到起点早于视口的长事件。
class IntervalIndex {
public:
    /// 接管事件数组，校验后按start/ID排序并建立子树最大终点；构建支持取消。
    explicit IntervalIndex(std::vector<Event> events = {}, const CancelFlag& cancel = {});
    /// 查询与半开range相交的事件，最多limit条；额外命中置truncated，返回指针借用索引存储。
    QueryResult query(TimeRange range, std::size_t limit = 10000) const;
    /// 只读借用按起点排序的事件数组；引用及内部指针依赖索引对象寿命。
    const std::vector<Event>& events() const { return events_; }
private:
    /// 递归构建[lo,hi)子树最大终点，node是数组树节点编号；分批检查取消。
    TimeNs build(std::size_t node, std::size_t lo, std::size_t hi, const CancelFlag& cancel);
    /// 查询[lo,hi)子树并累积out；利用maxEnd安全剪枝，避免漏掉跨视口长事件。
    void visit(std::size_t node, std::size_t lo, std::size_t hi, TimeRange range,
               std::size_t limit, QueryResult& out) const;
    /// 本索引拥有的事件数组，按start和ID排序。
    std::vector<Event> events_;
    /// 数组区间树的子树最大终点，用于安全剪枝跨视口长事件。
    std::vector<TimeNs> maxEnd_;
};

/// 一条源轨道及其索引/概览；UI显示行通过独立映射访问它。
struct Track {
    /// 源轨道显示名称，UI可过滤该名称但不改变轨道身份。
    std::string name;
    /// 本轨道拥有的区间索引，管理事件存储与可见区查询。
    IntervalIndex index;
    // 每桶记录与其相交的事件数，不是GPU利用率，也不用于精确统计。
    /// 每桶相交事件数，供密集视口LOD使用，不代表GPU利用率。
    std::vector<std::uint32_t> overviewCounts;
    /// 按Present分桶的最大帧间隔（纳秒），用于曲线保留尖峰。
    std::vector<TimeNs> frameMaxDuration; // 帧图概览：按Present时间分桶，值为最大帧间隔。
};

/// 随快照保存的输入字节摘要和质量计数，不含私有文件路径。
struct SourceInfo {
    /// 本次读取原始字节的SHA-256；未提供摘要时为空。
    std::string sha256;
    /// 解析器计数的数据记录总数，不含表头；与rejected共同描述输入质量。
    std::size_t records = 0;
    /// 因无效字段/数值被排除的记录数，报告保留该质量限制。
    std::size_t rejected = 0;
};

// Q05：完成构建后只通过shared_ptr<const TraceStore>发布；UI没有写入口。
/// 完成构建后经shared_ptr<const TraceStore>发布的数据仓库，读者不可修改。
struct TraceStore {
    /// 快照版本/请求代次标识，区分会话并参与缓存失效。
    std::uint64_t version = 0;
    /// 是否为教学生成数据，界面与报告须显式标识。
    bool synthetic = true;
    /// 是否采用帧语义，决定Present归属与完整间隔统计规则。
    bool frames = false;
    /// 输入摘要与记录质量信息，不存本机完整路径。
    SourceInfo input;
    /// 来源语义说明，报告不能依赖本机完整路径。
    std::string source = "教学模拟 seed 42";
    /// 解析或质量限制说明，供来源页及导出共同使用。
    std::vector<std::string> warnings;
    /// 全会话半开纳秒范围，作为视口导航边界。
    TimeRange bounds;
    /// 概览桶宽度（纳秒），最小1以避免除零。
    TimeNs bucketWidth = 1;
    /// 有效事件总数，与LOD绘制图元数量分开统计。
    std::size_t eventCount = 0;
    /// 源轨道数组，事件track及UI映射均以其下标定位。
    std::vector<Track> tracks;
    /// 由Event::name索引的名称字典，减少字符串重复。
    std::vector<std::string> names;
};
/// 只读会话所有权句柄；发布后无写入口，跨线程共享无需锁住事件数组。
using Snapshot = std::shared_ptr<const TraceStore>;
/// 接管事件与名称，分轨建索引和概览后发布const快照；来源/质量随数据保存，失败不发布半成品。
Snapshot buildStore(std::vector<Event> events, std::vector<std::string> tracks,
                    std::vector<std::string> names, std::uint64_t version,
                    const CancelFlag& cancel = {}, const Progress& progress = {},
                    bool synthetic = true, bool frames = false, std::string source = "教学模拟 seed 42",
                    std::vector<std::string> warnings = {}, SourceInfo input = {});

// Q04：最新请求邮箱，最多一个待执行请求；丢弃的是中间任务请求，不是原始事件。
/// 串行协调线程使用的单项待办邮箱，替换中间请求而不丢原始数据。
template<class T> class LatestRequest {
public:
    /// 替换唯一待办，舍弃中间请求；邮箱由协调线程串行访问，不是线程安全队列。
    void replace(T value) { pending_ = std::make_unique<T>(std::move(value)); }
    /// 移出唯一待办并清空邮箱，unique_ptr转移所有权；空值表示无任务。
    std::unique_ptr<T> take() { return std::move(pending_); }
    /// 判断是否有待办但不移出任务，仍要求调用方串行访问。
    bool hasValue() const { return bool(pending_); }
private:
    /// 唯一待执行请求，新请求替换旧待办以避免无界排队。
    std::unique_ptr<T> pending_;
};
} // namespace gpuview
