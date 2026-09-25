/// @file event_analysis.h
/// @brief 事件搜索、排序与名称聚合契约；核心层不依赖Qt，输入和输出共享只读快照。
#pragma once
#include "core/trace_store.h"
#include <optional>
namespace gpuview {
/// 一次搜索的冻结条件；名称为区分大小写的UTF-8字面子串，不解释正则。
struct EventFilter {
    /// 名称包含条件，空字符串匹配全部名称。
    std::string text;
    /// 纳入范围，Trace按区间相交，帧按Present时刻归属。
    TimeRange range;
    /// 参与搜索的源轨道ID，空列表表示不搜索任何轨道。
    std::vector<std::uint32_t> tracks;
    /// 原始完整时长下限（纳秒），含边界。
    TimeNs minimum = 0;
    /// 原始完整时长上限（纳秒），空值表示无限，含边界。
    std::optional<TimeNs> maximum;
    /// 事件列排序：0 ID、1名称、2轨道、3起点、4完整时长、5范围贡献。
    int eventColumn = 3;
    /// 事件主键是否降序，同值按稳定ID升序。
    bool eventDescending = false;
    /// 汇总列排序：0名称、1次数、2总时长、3均值、4最大时长。
    int groupColumn = 2;
    /// 汇总默认按总时长降序；同值按名称确定顺序。
    bool groupDescending = true;
};
/// 一条搜索结果，不复制原始事件，source负责延长指针寿命。
struct EventMatch {
    /// 非拥有原始事件指针，禁止脱离分析快照保存。
    const Event* event = nullptr;
    /// 范围贡献（纳秒）：Trace裁剪后的交集，帧为完整间隔。
    TimeNs contribution = 0;
};
/// 同名事件跨当前所选轨道的精确汇总，不表达调用栈层级。
struct NameSummary {
    /// UTF-8事件名称，同名字典项合并为一组。
    std::string name;
    /// 匹配事件数量，不受显示行数上限截断。
    std::size_t count = 0;
    /// 范围贡献之和（毫秒）；并发求和可超过墙钟，不是利用率。
    long double totalMs = 0;
    /// 最大单项范围贡献（纳秒）。
    TimeNs maximum = 0;
    /// 最大贡献事件，用于从汇总行定位；同值选较小ID。
    const Event* representative = nullptr;
};
/// 搜索结果发布单元，保留筛选条件及源所有权供UI一致读取。
struct EventAnalysis {
    /// 共享只读源快照，保护rows/groups内所有指针。
    Snapshot source;
    /// 产生本结果的条件，不与正在编辑的新条件混用。
    EventFilter filter;
    /// 完整匹配事件行，没有静默上限。
    std::vector<EventMatch> rows;
    /// 名称汇总行，统计全部匹配事件。
    std::vector<NameSummary> groups;
    /// 按ID排序的ID到当前行映射，避免GUI线性扫描百万结果。
    std::vector<std::pair<std::uint64_t,int>> idRows;
    /// 二分查稳定ID；不存在返回-1。
    int rowForId(std::uint64_t id) const;
};
/// 跨线程只读结果句柄，发布后只替换指针不修改内部数据。
using EventAnalysisPtr = std::shared_ptr<const EventAnalysis>;
/// 后台扫描/聚合/排序，非法条件抛异常；各长循环均支持协作取消。
EventAnalysisPtr analyzeEvents(Snapshot source, EventFilter filter, const CancelFlag& cancel = {});
}
