#pragma once
#include "core/render_query.h"
#include <QWidget>
#include <QPoint>
namespace gpuview {
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setSnapshot(Snapshot snapshot);
    void resetViewport();
    void setFirstTrack(int track);
    void setTracks(std::vector<std::uint32_t> tracks);
    const std::vector<std::uint32_t>& tracks() const { return tracks_; }
    void zoomSelection();
    void previousView();
    void selectEvent(const Event& event);
    void selectRange(TimeRange range);
    void focusEvent(const Event& event);
    void showRange(TimeRange range);
    std::optional<Event> selectedEvent() const { return selectedEvent_; }
    void setCacheEnabled(bool enabled) { cacheEnabled_ = enabled; cache_.clear(); }
    TimeRange visibleRange() const { return viewport_.range(); }
    std::size_t primitiveCount() const { return lastPrimitives_; }
    double lastPaintMs() const { return lastPaintMs_; }
signals:
    void eventPicked(qulonglong id, const QString& details);
    void rangeSelected(qint64 begin, qint64 end);
    void diagnosticsChanged(const QString& text);
    void viewportChanged(qint64 begin, qint64 end);
    void selectionCleared();
    void trackScrollChanged(int firstTrack, int maximum, int pageStep);
protected:
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    int visibleTrackCount() const;
    void syncTrackScroll(int requestedTrack);
    void publishEvent(const Event& event);
    void rememberView();
    int trackAt(int y) const;
    TimeNs timeAt(double x) const;
    void pick(const QPoint& point);
    static constexpr int gutter = 150;
    static constexpr int top = 40;
    static constexpr int row = 32;
    Snapshot snapshot_;
    std::vector<std::uint32_t> tracks_;
    std::vector<TimeRange> history_;
    std::optional<Event> selectedEvent_;
    QPoint hover_{-1, -1};
    bool cacheEnabled_ = true;
    TimeViewport viewport_;
    RenderCache cache_;
    std::uint32_t firstTrack_ = 0;
    QPoint press_;
    QPoint previous_;
    bool panning_ = false;
    bool selecting_ = false;
    std::optional<TimeRange> selection_;
    std::size_t lastPrimitives_ = 0;
    double lastPaintMs_ = 0;
};
}
