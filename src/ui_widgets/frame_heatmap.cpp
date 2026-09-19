/// @file src/ui_widgets/frame_heatmap.cpp
/// @brief 全会话1秒最大帧间隔概览；GUI只读取后台投影数据，点击发出精确秒桶选区。
#include "ui_widgets/frame_heatmap.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <algorithm>
#include <cmath>
namespace gpuview {
/// 把横坐标映射为会话内1秒桶的半开纳秒范围；无数据时返回空范围。
TimeRange FrameHeatmap::bucketAt(int x) const {
    if(!analysis_) return {0,0};
    const auto bounds=analysis_->source->bounds;
    const double fraction=std::clamp(double(x-150)/std::max(1,width()-162),0.0,1.0);
    const auto time=std::min(bounds.end-1,bounds.begin+TimeNs(fraction*(bounds.end-bounds.begin)));
    const auto begin=time/1000000000*1000000000;
    return {begin,begin+std::min<TimeNs>(1000000000,bounds.end-begin)};
}
/// 将后台max概览压到有限像素宽度；缺失画N/A，颜色表示帧间隔而非GPU利用率。
void FrameHeatmap::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(),QColor("#15212d")); p.setPen(QColor("#b9cbd9"));
    p.drawText(10,18,QStringLiteral("全会话热力图 · 1秒桶最大帧间隔概览（ms） · 点击选区"));
    p.drawText(10,70,QStringLiteral("灰：N/A   绿：≤16.67   黄：≤33.33   橙：≤50   红：>50（非GPU利用率）"));
    const int pixels=std::max(1,width()-162); p.fillRect(150,27,pixels,22,QColor("#34404b"));
    if(!analysis_) return;
    const auto bounds=analysis_->source->bounds; const double scale=double(pixels)/(bounds.end-bounds.begin);
    // 多桶投到同一像素时保留最大值，不能让后画的低值遮掉尖峰。
    std::vector<TimeNs> values(std::size_t(pixels),0);
    for(std::size_t i=0;i<analysis_->heatOverview.size();++i) {
        const int first=int(i*std::size_t(pixels)/analysis_->heatOverview.size());
        const int last=std::min(pixels-1,int(((i+1)*std::size_t(pixels)+analysis_->heatOverview.size()-1)/analysis_->heatOverview.size())-1);
        for(int x=first;x<=last;++x) values[std::size_t(x)]=std::max(values[std::size_t(x)],analysis_->heatOverview[i]);
    }
    for(int x=0;x<pixels;++x) if(values[std::size_t(x)]) {
        const double ms=double(values[std::size_t(x)])/1e6;
        p.fillRect(150+x,27,1,22,QColor(ms<=16.67?"#3cd7b1":ms<=33.33?"#e4d15a":ms<=50?"#ee9b4b":"#e66161"));
    }
    p.setPen(QPen(QColor("#ffffff"),2)); p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(150+(analysis_->range.begin-bounds.begin)*scale,26,
        (analysis_->range.end-analysis_->range.begin)*scale,24));
}
/// 将有效左击映射为1秒桶选区并发出rangePicked，不修改全会话热力数据。
void FrameHeatmap::mousePressEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton || event->position().x()<150 || event->position().y()<27 || event->position().y()>49) return;
    const auto range=bucketAt(int(event->position().x()));
    if(range.end>range.begin) emit rangePicked(range.begin,range.end);
}
/// 按鼠标时刻查原始稀疏秒桶显示数量/max，区分精确桶值与概览像素聚合。
void FrameHeatmap::mouseMoveEvent(QMouseEvent* event) {
    if(!analysis_ || event->position().x()<150) return;
    const auto range=bucketAt(int(event->position().x()));
    // 稀疏秒桶按起点有序；二分查询鼠标所指原始桶，不把概览像素误作单桶统计。
    const auto it=std::lower_bound(analysis_->heat.begin(),analysis_->heat.end(),range.begin,[](const HeatBin& b,TimeNs t) { return b.begin<t; });
    const QString value=it==analysis_->heat.end() || it->begin!=range.begin ? QStringLiteral("N/A（无有效帧）") :
        QStringLiteral("最大 %1 ms / %2 帧").arg(double(it->maximum)/1e6,0,'f',3).arg(it->count);
    QToolTip::showText(event->globalPosition().toPoint(),QStringLiteral("[%1, %2) s\n%3\n全览像素可能合并多个桶；点击按光标时刻选择1秒桶。")
        .arg(double(range.begin)/1e9,0,'f',3).arg(double(range.end)/1e9,0,'f',3).arg(value),this);
}
}
