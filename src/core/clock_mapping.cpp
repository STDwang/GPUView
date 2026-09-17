#include "core/clock_mapping.h"
#include <cmath>
#include <limits>
namespace gpuview {
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
std::optional<TimeNs> ClockMapping::map(TimeNs tick) const {
    if (!valid_) return std::nullopt;
    const auto value = std::round(origin_.sessionNs + (static_cast<long double>(tick) - origin_.sourceTick) * scale_);
    if (!std::isfinite(value) || value < std::numeric_limits<TimeNs>::min() ||
        value >= static_cast<long double>(std::numeric_limits<TimeNs>::max())) return std::nullopt;
    return static_cast<TimeNs>(value);
}
} // namespace gpuview
