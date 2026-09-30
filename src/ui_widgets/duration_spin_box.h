/// @file duration_spin_box.h
/// @brief 毫秒条件输入框；沿用Qt输入/步进逻辑，仅补画深色主题的清晰箭头。
#pragma once
#include <QDoubleSpinBox>
namespace gpuview {
/// 不改动Qt命中与输入校验，以标准子控件几何绘制缩放友好的矢量箭头。
class DurationSpinBox : public QDoubleSpinBox {
public:
    /// 沿用Qt父子所有权和默认构造参数。
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    /// 先绘制Qt输入框，再补画上下箭头；方向和可用性取自Qt步进状态。
    void paintEvent(QPaintEvent* event) override;
};
}
