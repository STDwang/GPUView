/// @file src/core/clock_mapping.h
/// @brief 基于同次采集锚点的线性时钟映射；未知或非法映射不伪造零时刻。
#pragma once
#include "core/trace_store.h"
#include <optional>
namespace gpuview {
/// 同次采集中源时钟与会话时钟的一对对应读数。
struct ClockAnchor {
    /// 源时钟锚点读数，其刻度由两点映射比例转换。
    TimeNs sourceTick;
    /// 同一采集锚点对应的会话纳秒时刻。
    TimeNs sessionNs;
};
// Q09：映射必须由同一次采集的锚点建立，不能假设两个源的零点相同。
/// 带有效性及不确定度的两点线性时钟映射；只做算法转换，不声称已接入真实双源采集。
class ClockMapping {
public:
    /// 用两个有序采集锚点建立线性映射并记录纳秒不确定度；非法锚点拒绝，不能假设两个时钟同零点。
    static ClockMapping fromAnchors(ClockAnchor a, ClockAnchor b, TimeNs uncertaintyNs);
    /// 把源时钟tick映射为会话纳秒；未建立映射或结果越界时返回nullopt而非伪造零时刻。
    std::optional<TimeNs> map(TimeNs sourceTick) const;
    /// 返回采集方提供的映射不确定度（纳秒），不是算法自动测得的误差。
    TimeNs uncertaintyNs() const { return uncertaintyNs_; }
private:
    /// 是否已有合法映射；false时map返回nullopt。
    bool valid_ = false;
    /// 线性映射原点，先减源tick再缩放以减少大数精度损失。
    ClockAnchor origin_{};
    /// 每个源tick对应的会话纳秒比例，用long double计算。
    long double scale_ = 1;
    /// 采集方提供的不确定度（纳秒），不能当作无误差对齐。
    TimeNs uncertaintyNs_ = 0;
};
} // namespace gpuview
