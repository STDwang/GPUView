/// @file src/ui_widgets/frame_heatmap.h
/// @brief 全会话1秒最大帧间隔概览；GUI只读取后台投影数据，点击发出精确秒桶选区。
#pragma once
#include "core/frame_analysis.h"
#include <QWidget>
namespace gpuview {
/// 全会话帧热力概览控件，颜色表示桶最大间隔而非GPU利用率。
class FrameHeatmap : public QWidget {
    Q_OBJECT
public:
    /// 设置概览高度并启用悬停；聚合由后台完成，控件仅绘制和命中。
    explicit FrameHeatmap(QWidget* parent = nullptr) : QWidget(parent) { setFixedHeight(78); setMouseTracking(true); }
    /// GUI线程绑定只读分析并刷新显示；空值表示当前分析未就绪，不继续展示旧范围数据。
    void setAnalysis(FrameAnalysisPtr analysis) { analysis_=std::move(analysis); update(); }
    /// 把横坐标映射为会话内1秒桶的半开纳秒范围；无数据时返回空范围。
    TimeRange bucketAt(int x) const;
signals:
    /// 发布点击桶的纳秒范围，使明细和统计选择同一时间段。
    void rangePicked(qint64 begin, qint64 end);
protected:
    /// 将后台max概览压到有限像素宽度；缺失画N/A，颜色表示帧间隔而非GPU利用率。
    void paintEvent(QPaintEvent*) override;
    /// 将有效左击映射为1秒桶选区并发出rangePicked，不修改全会话热力数据。
    void mousePressEvent(QMouseEvent*) override;
    /// 按鼠标时刻查原始稀疏秒桶显示数量/max，区分精确桶值与概览像素聚合。
    void mouseMoveEvent(QMouseEvent*) override;
private:
    /// 共享只读分析，延长源数据寿命；GUI发布时整体替换。
    FrameAnalysisPtr analysis_;
};
}
