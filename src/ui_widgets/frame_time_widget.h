/// @file src/ui_widgets/frame_time_widget.h
/// @brief 帧间隔曲线及点击拾取；按像素最大值保留尖峰，帧间隔不能解释为GPU执行时长。
#pragma once
#include "core/trace_store.h"
#include <QWidget>
namespace gpuview {
/// 帧间隔曲线控件，使用共享快照且只绘制当前时间范围。
class FrameTimeWidget : public QWidget {
    Q_OBJECT
public:
    /// 设置帧曲线高度；只消费已发布快照，不直接解析CSV。
    explicit FrameTimeWidget(QWidget* parent = nullptr) : QWidget(parent) { setMinimumHeight(150); setMaximumHeight(210); }
    /// 绑定当前源轨道的只读快照并恢复全范围；track由上层有效分组提供。
    void setData(Snapshot snapshot, std::uint32_t track) { snapshot_=std::move(snapshot); track_=track; if(snapshot_) range_=snapshot_->bounds; update(); }
    /// 同步可见纳秒范围并请求重绘，不在此重新计算精确统计。
    void setRange(TimeRange range) { range_=range; update(); }
signals:
    /// 发布命中原始帧供详情定位；帧间隔不能解释为GPU执行时间。
    void framePicked(const gpuview::Event& event);
protected:
    /// 按像素取最大帧间隔绘制曲线和60FPS预算线；密集时用概览，不能解读为GPU时长。
    void paintEvent(QPaintEvent*) override;
    /// 查询点击时刻附近的源帧并发出framePicked供上层联动定位。
    void mousePressEvent(QMouseEvent*) override;
private:
    /// 当前只读快照，持有期间源事件及索引保持有效。
    Snapshot snapshot_;
    /// 帧曲线展示的源轨道ID，由上层分组选择提供。
    std::uint32_t track_=0;
    /// 当前可见半开纳秒范围，由视口或上层同步。
    TimeRange range_;
};
}
