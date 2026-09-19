/// @file src/core/render_query.h
/// @brief 可见几何查询、LOD、单项几何缓存和时间视口；不依赖Qt，方便独立验证。
#pragma once
#include "core/trace_store.h"
#include <optional>

namespace gpuview {
/// 完整几何缓存键，包含版本、时间范围、宽度与轨道映射，防止错误复用。
struct RenderKey {
    /// 快照版本/请求代次标识，区分会话并参与缓存失效。
    std::uint64_t version;
    /// 本次统计/几何查询的半开纳秒范围。
    TimeRange range;
    /// 绘图区宽度（逻辑像素），决定时间投影与LOD粒度。
    int width;
    /// 首个可见行；trackIds为空时也是源轨道ID，非空时是该映射数组的下标。
    std::uint32_t firstTrack;
    /// 本批次可见轨道数量，限制纵向查询和绘制成本。
    std::uint32_t trackCount;
    /// 过滤后的有序源轨道ID；为空时兼容连续轨道模式。
    std::vector<std::uint32_t> trackIds; // 空值兼容连续轨道；UI传入过滤后的有序ID。
    /// 比较组成状态的全部字段，用于时间范围一致性或完整缓存键判等。
    bool operator==(const RenderKey& other) const;
};
/// 一个自绘几何元素，可对应单事件或LOD聚合桶，不是QObject/QGraphicsItem。
struct Primitive {
    /// 相对绘图区左边界的图元横坐标，单位逻辑像素。
    double x;
    /// 图元宽度，单位逻辑像素；聚合桶宽度不等于单个事件时长。
    double width;
    /// 源轨道数组的索引，不是过滤后的可见行号。
    std::uint32_t track;
    /// 精确图元对应的稳定事件ID，聚合项不能用于直接确认事件身份。
    std::uint64_t eventId;
    /// 纳入当前结果的样本/事件数，不是GPU利用率。
    std::uint32_t count;
    /// true表示LOD聚合图元，false表示精确事件几何。
    bool aggregate;
};
/// 一次可见区查询产生的有界几何与诊断信息。
struct RenderBatch {
    /// 紧凑待绘制几何数组，不为每项分配QObject。
    std::vector<Primitive> primitives;
    /// 是否采用概览近似；精确统计仍查询源数据。
    bool approximate = false;
    /// 查询访问的树节点数量，用于算法成本诊断。
    std::size_t visitedNodes = 0;
};
/// 根据时间/轨道/宽度生成几何；密集数据用有界概览，近似图元不用于精确统计。
RenderBatch makeRenderBatch(const TraceStore& store, const RenderKey& key);

// 缓存纯几何，不缓存颜色/字体/DPR栅格；主题变更重绘即可，不需要清空几何。
/// 只保留一个几何批次并持有其源快照；使用完整键判定是否可复用。
class RenderCache {
public:
    /// 同快照同键复用几何，否则重建；返回引用在下一次get或clear后可能失效。
    const RenderBatch& get(const Snapshot& store, const RenderKey& key);
    /// 释放缓存快照、键和几何；保留累计命中计数供诊断。
    void clear();
    /// 返回累计命中次数，供缓存开关对照测量。
    std::uint64_t hits() const { return hits_; }
    /// 返回累计重建次数，不能直接等同卡顿次数。
    std::uint64_t misses() const { return misses_; }
private:
    /// 缓存持有的只读快照，同时保证源数据寿命。
    Snapshot owner_;
    /// 最近几何请求的完整键，空值表示缓存未建立。
    std::optional<RenderKey> key_;
    /// 单项渲染批次缓存，下次未命中时整体替换。
    RenderBatch batch_;
    /// 累计缓存命中次数，用于性能诊断。
    std::uint64_t hits_ = 0;
    /// 累计几何重建次数，用于失效与复用分析。
    std::uint64_t misses_ = 0;
};
/// 独立于Qt的纳秒视口，集中处理缩放锚点、平移及边界夹紧。
class TimeViewport {
public:
    /// 以纳秒边界初始化视口，所有缩放和平移受该边界约束。
    explicit TimeViewport(TimeRange bounds = {0, 1}) { reset(bounds); }
    /// 重设总边界并恢复全览；负起点、空或反向范围抛invalid_argument。
    void reset(TimeRange bounds);
    /// 按factor缩放时长，尽量保持相对位置anchor所指时刻不变；非法参数忽略。
    void zoom(double factor, double anchor);
    /// 按当前可见时长的fraction平移，最终范围夹紧到数据边界。
    void pan(double fraction);
    /// 显示指定纳秒范围；忽略空范围，内部统一夹紧边界。
    void show(TimeRange range) { if (range.end > range.begin) set(range.begin, range.end - range.begin); }
    /// 返回当前半开纳秒范围，不暴露可写内部状态。
    TimeRange range() const { return range_; }
private:
    /// 统一夹紧起点与时长，避免缩放和平移越过全会话边界。
    void set(TimeNs begin, TimeNs duration);
    /// 视口允许导航的全会话纳秒边界。
    TimeRange bounds_;
    /// 当前可见半开纳秒范围，由视口或上层同步。
    TimeRange range_;
};
} // namespace gpuview
