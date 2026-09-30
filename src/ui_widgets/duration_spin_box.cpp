/// @file duration_spin_box.cpp
/// @brief QStyle提供箭头按钮命中区域，QPainter绘制逻辑像素折线，避免额外图像插件。
#include "ui_widgets/duration_spin_box.h"
#include <QPainter>
#include <QStyleOptionSpinBox>
namespace gpuview {
/// 样式表定制按钮背景后平台箭头可能消失，补画箭头不重写按钮、键盘和范围语义。
void DurationSpinBox::paintEvent(QPaintEvent* event) {
    QDoubleSpinBox::paintEvent(event);
    QStyleOptionSpinBox option; initStyleOption(&option);
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    // 所有坐标来自QStyle子控件矩形，适配窗口缩放、字体尺寸和左右布局。
    const auto arrow=[&](QStyle::SubControl control,bool up,bool enabled) {
        const auto bounds=style()->subControlRect(QStyle::CC_SpinBox,&option,control,this);
        if(bounds.isEmpty()) return;
        const QPointF center=bounds.center(); const double direction=up?-1.0:1.0;
        painter.setPen(QPen(QColor(enabled?"#0b121a":"#587080"),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPolygonF points; points<<center+QPointF(-3,-direction)<<center+QPointF(0,2*direction)<<center+QPointF(3,-direction);
        painter.drawPolyline(points);
    };
    arrow(QStyle::SC_SpinBoxUp,true,isEnabled() && option.stepEnabled.testFlag(StepUpEnabled));
    arrow(QStyle::SC_SpinBoxDown,false,isEnabled() && option.stepEnabled.testFlag(StepDownEnabled));
}
}
