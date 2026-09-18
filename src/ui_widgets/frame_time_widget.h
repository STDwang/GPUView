#pragma once
#include "core/trace_store.h"
#include <QWidget>
namespace gpuview {
class FrameTimeWidget : public QWidget {
    Q_OBJECT
public:
    explicit FrameTimeWidget(QWidget* parent = nullptr) : QWidget(parent) { setMinimumHeight(150); setMaximumHeight(210); }
    void setData(Snapshot snapshot, std::uint32_t track) { snapshot_=std::move(snapshot); track_=track; if(snapshot_) range_=snapshot_->bounds; update(); }
    void setRange(TimeRange range) { range_=range; update(); }
signals:
    void framePicked(const gpuview::Event& event);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    Snapshot snapshot_;
    std::uint32_t track_=0;
    TimeRange range_;
};
}
