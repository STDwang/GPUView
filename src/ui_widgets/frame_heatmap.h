#pragma once
#include "core/frame_analysis.h"
#include <QWidget>
namespace gpuview {
class FrameHeatmap : public QWidget {
    Q_OBJECT
public:
    explicit FrameHeatmap(QWidget* parent = nullptr) : QWidget(parent) { setFixedHeight(78); setMouseTracking(true); }
    void setAnalysis(FrameAnalysisPtr analysis) { analysis_=std::move(analysis); update(); }
    TimeRange bucketAt(int x) const;
signals:
    void rangePicked(qint64 begin, qint64 end);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    FrameAnalysisPtr analysis_;
};
}
