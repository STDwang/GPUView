#pragma once
#include "core/trace_store.h"
#include <optional>
namespace gpuview {
struct ClockAnchor { TimeNs sourceTick; TimeNs sessionNs; };
// Q09：映射必须由同一次采集的锚点建立，不能假设两个源的零点相同。
class ClockMapping {
public:
    static ClockMapping fromAnchors(ClockAnchor a, ClockAnchor b, TimeNs uncertaintyNs);
    std::optional<TimeNs> map(TimeNs sourceTick) const;
    TimeNs uncertaintyNs() const { return uncertaintyNs_; }
private:
    bool valid_ = false;
    ClockAnchor origin_{};
    long double scale_ = 1;
    TimeNs uncertaintyNs_ = 0;
};
} // namespace gpuview
