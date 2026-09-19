/// @file src/core/clock_mapping.cpp
/// @brief 基于同次采集锚点的线性时钟映射；未知或非法映射不伪造零时刻。
#include "core/clock_mapping.h"
#include <cmath>
#include <limits>
namespace gpuview {
/// 用两个有序采集锚点建立线性映射并记录纳秒不确定度；非法锚点拒绝，不能假设两个时钟同零点。
ClockMapping ClockMapping::fromAnchors(ClockAnchor a, ClockAnchor b, TimeNs uncertaintyNs) {
    if (b.sourceTick <= a.sourceTick || b.sessionNs <= a.sessionNs || uncertaintyNs < 0)
        throw std::invalid_argument("invalid clock anchors");
    ClockMapping result;
    result.valid_ = true;
    result.origin_ = a;
    result.scale_ = (static_cast<long double>(b.sessionNs) - a.sessionNs) /
                    (static_cast<long double>(b.sourceTick) - a.sourceTick);
    result.uncertaintyNs_ = uncertaintyNs;
    return result;
}
/// 把源时钟tick映射为会话纳秒；未建立映射或结果越界时返回nullopt而非伪造零时刻。
std::optional<TimeNs> ClockMapping::map(TimeNs tick) const {
    if (!valid_) return std::nullopt;
    const auto value = std::round(origin_.sessionNs + (static_cast<long double>(tick) - origin_.sourceTick) * scale_);
    if (!std::isfinite(value) || value < std::numeric_limits<TimeNs>::min() ||
        value >= static_cast<long double>(std::numeric_limits<TimeNs>::max())) return std::nullopt;
    return static_cast<TimeNs>(value);
}
} // namespace gpuview
