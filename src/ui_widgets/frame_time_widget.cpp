/// @file src/ui_widgets/frame_time_widget.cpp
/// @brief 帧间隔曲线及点击拾取；按像素最大值保留尖峰，帧间隔不能解释为GPU执行时长。
#include "ui_widgets/frame_time_widget.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
namespace gpuview {
/// 按像素取最大帧间隔绘制曲线和60FPS预算线；密集时用概览，不能解读为GPU时长。
void FrameTimeWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.fillRect(rect(), QColor("#15212d"));
    if(!snapshot_ || track_>=snapshot_->tracks.size()) return;
    const auto& track=snapshot_->tracks[track_];
    const int plotWidth=std::max(1,width()-162), plotHeight=height()-45;
    const auto exact=track.index.query(range_,std::size_t(plotWidth)*2);
    std::vector<double> pixels(std::size_t(plotWidth),0);
    // 将纳秒时刻/间隔投到当前像素列并取max；同列短帧不会覆盖较长尖峰。
    auto add=[&](TimeNs timestamp,TimeNs duration) {
        if(timestamp<range_.begin || timestamp>=range_.end) return;
        const int x=std::clamp(int(double(timestamp-range_.begin)/(range_.end-range_.begin)*plotWidth),0,plotWidth-1);
        pixels[std::size_t(x)]=std::max(pixels[std::size_t(x)],double(duration)/1e6);
    };
    if(!exact.truncated) { for(const auto* e:exact.events) add(e->start,e->duration); }
    else for(std::size_t i=0;i<track.frameMaxDuration.size();++i) {
        const TimeNs begin=TimeNs(i)*snapshot_->bucketWidth, end=begin+std::min(snapshot_->bounds.end-begin,snapshot_->bucketWidth);
        if(begin<range_.end && end>range_.begin) add(std::max(begin,range_.begin),track.frameMaxDuration[i]);
    }
    const double maximum=std::max(33.334,*std::max_element(pixels.begin(),pixels.end()));
    painter.setPen(QColor("#b9cbd9"));
    painter.drawText(10,20,exact.truncated?QStringLiteral("帧间隔 / 桶最大值概览"):QStringLiteral("帧间隔 / 每像素最大值"));
    painter.drawText(10,45,QString::number(maximum,'f',2)+" ms");
    const int budgetY=height()-10-int(16.667/maximum*plotHeight);
    painter.setPen(QPen(QColor("#a6b4c4"),1,Qt::DashLine)); painter.drawLine(150,budgetY,width()-12,budgetY);
    painter.drawText(10,budgetY,QStringLiteral("60FPS预算 16.67"));
    painter.setPen(QColor("#55dabb"));
    for(int x=0;x<plotWidth;++x) if(pixels[std::size_t(x)]>0)
        painter.drawLine(150+x,height()-10,150+x,height()-10-int(pixels[std::size_t(x)]/maximum*plotHeight));
}
/// 查询点击时刻附近的源帧并发出framePicked供上层联动定位。
void FrameTimeWidget::mousePressEvent(QMouseEvent* event) {
    if(!snapshot_ || track_>=snapshot_->tracks.size() || event->position().x()<150) return;
    const double fraction=std::clamp((event->position().x()-150)/std::max(1,width()-162),0.0,1.0);
    const auto time=range_.begin+TimeNs(fraction*(range_.end-range_.begin));
    const auto& events=snapshot_->tracks[track_].index.events();
    // 源事件按Present起点有序，二分找到光标时间附近的帧再由后续逻辑比较邻居。
    auto it=std::lower_bound(events.begin(),events.end(),time,[](const Event& e,TimeNs t) { return e.start<t; });
    if(it==events.end()) { if(events.empty()) return; --it; }
    else if(it!=events.begin() && time-(it-1)->start<it->start-time) --it;
    emit framePicked(*it);
}
}
