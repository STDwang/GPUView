#include "ui_widgets/timeline_widget.h"
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace gpuview {
TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}
void TimelineWidget::setSnapshot(Snapshot snapshot) {
    snapshot_ = std::move(snapshot);
    firstTrack_ = 0;
    selection_.reset();
    cache_.clear();
    resetViewport();
}
void TimelineWidget::resetViewport() {
    if (snapshot_) viewport_.reset(snapshot_->bounds);
    selection_.reset();
    update();
}
void TimelineWidget::setFirstTrack(int track) {
    firstTrack_ = std::uint32_t(std::max(0, track));
    update();
}
void TimelineWidget::paintEvent(QPaintEvent*) {
    QElapsedTimer elapsed;
    elapsed.start();
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#101923"));
    if (!snapshot_) {
        painter.setPen(QColor("#9dafbd"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("选择 10万 / 100万 教学事件开始\n可取消后台构建 · 不含真实采集数据"));
        return;
    }
    const int plotWidth = std::max(1, width() - gutter - 12);
    const auto view = viewport_.range();
    const auto visibleTracks = std::uint32_t(std::max(1, (height() - top) / row));
    const auto& batch = cache_.get(snapshot_, {snapshot_->version, view, plotWidth, firstTrack_, visibleTracks});
    lastPrimitives_ = batch.primitives.size();
    painter.setPen(QColor("#354252"));
    for (int tick = 0; tick <= 8; ++tick) {
        const int x = gutter + plotWidth * tick / 8;
        painter.drawLine(x, top - 5, x, height());
        painter.setPen(QColor("#a9bac8"));
        const auto time = view.begin + (view.end - view.begin) * tick / 8;
        const auto label = QString::number(double(time) / 1e6, 'f', 1) + " ms";
        const auto labelWidth = painter.fontMetrics().horizontalAdvance(label);
        painter.drawText(std::clamp(x - labelWidth / 2, gutter, width() - labelWidth - 4), 23, label);
        painter.setPen(QColor("#273442"));
    }
    for (std::uint32_t t = firstTrack_; t < std::min(std::uint32_t(snapshot_->tracks.size()), firstTrack_ + visibleTracks); ++t) {
        const int y = top + int(t - firstTrack_) * row;
        painter.fillRect(0, y, gutter - 5, row, QColor(t % 2 ? "#182431" : "#15212d"));
        painter.setPen(QColor("#b9cbd9"));
        painter.drawText(QRect(12, y, gutter - 15, row), Qt::AlignVCenter, QString::fromStdString(snapshot_->tracks[t].name));
    }
    painter.save();
    painter.setClipRect(gutter, top, plotWidth, height() - top);
    for (const auto& primitive : batch.primitives) {
        const int y = top + int(primitive.track - firstTrack_) * row + 5;
        QColor color = primitive.track < 8 ? QColor("#67a6e8") : QColor("#3cd7b1");
        if (primitive.aggregate) color.setAlpha(std::clamp(55 + int(std::log2(primitive.count + 1) * 30), 55, 240));
        painter.fillRect(QRectF(gutter + primitive.x, y, primitive.width, row - 10), color);
    }
    if (selection_) {
        const double scale = plotWidth / double(view.end - view.begin);
        painter.fillRect(QRectF(gutter + double(selection_->begin - view.begin) * scale, top,
                               double(selection_->end - selection_->begin) * scale, height() - top), QColor(250, 190, 80, 55));
    }
    painter.restore();
    lastPaintMs_ = elapsed.nsecsElapsed() / 1e6;
    // 只报告一次绘制的CPU时间；它不是屏幕呈现FPS。
    emit diagnosticsChanged(QStringLiteral("图元 %1  |  CPU paint %2 ms  |  cache %3/%4  |  %5")
        .arg(lastPrimitives_).arg(lastPaintMs_, 0, 'f', 2).arg(cache_.hits()).arg(cache_.misses())
        .arg(batch.approximate ? QStringLiteral("LOD概览（近似）") : QStringLiteral("精确区间")));
}
TimeNs TimelineWidget::timeAt(double x) const {
    const auto view = viewport_.range();
    const double fraction = std::clamp((x - gutter) / std::max(1, width() - gutter - 12), 0.0, 1.0);
    return view.begin + TimeNs(fraction * double(view.end - view.begin));
}
void TimelineWidget::wheelEvent(QWheelEvent* event) {
    if (!snapshot_) return;
    const double anchor = (event->position().x() - gutter) / std::max(1, width() - gutter - 12);
    viewport_.zoom(std::pow(1.25, event->angleDelta().y() / 120.0), anchor);
    update();
    event->accept();
}
void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (!snapshot_ || event->position().x() < gutter || event->position().y() < top) return;
    setFocus();
    press_ = previous_ = event->position().toPoint();
    panning_ = event->button() == Qt::MiddleButton;
    selecting_ = event->button() == Qt::LeftButton;
    if (selecting_) selection_.reset();
}
void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!snapshot_) return;
    const auto point = event->position().toPoint();
    if (panning_) {
        viewport_.pan(double(previous_.x() - point.x()) / std::max(1, width() - gutter - 12));
        previous_ = point;
        update();
    } else if (selecting_) {
        const auto a = timeAt(press_.x()), b = timeAt(point.x());
        selection_ = TimeRange{std::min(a, b), std::max(a, b)};
        update();
    }
}
void TimelineWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!snapshot_) return;
    if (selecting_) {
        if ((event->position().toPoint() - press_).manhattanLength() < 4) {
            selection_.reset();
            pick(event->position().toPoint());
        } else if (selection_ && selection_->end > selection_->begin)
            emit rangeSelected(selection_->begin, selection_->end);
    }
    panning_ = selecting_ = false;
    update();
}
void TimelineWidget::pick(const QPoint& point) {
    if (point.y() < top) return;
    const auto track = firstTrack_ + std::uint32_t((point.y() - top) / row);
    if (track >= snapshot_->tracks.size()) return;
    const auto time = timeAt(point.x());
    const auto query = snapshot_->tracks[track].index.query({time, time + 1}, 1);
    if (query.events.empty()) { emit eventPicked(0, QStringLiteral("该位置没有精确事件；概览桶不能替代原始事件")); return; }
    const auto& e = *query.events.front();
    emit eventPicked(e.id, QStringLiteral("事件 #%1\n名称：%2\n轨道：%3\n开始：%4 ms\n持续：%5 ms\n来源：教学模拟\n%6")
        .arg(e.id).arg(QString::fromStdString(snapshot_->names[e.name])).arg(e.track)
        .arg(double(e.start) / 1e6, 0, 'f', 6).arg(double(e.duration) / 1e6, 0, 'f', 6)
        .arg(query.truncated ? QStringLiteral("当前位置有重叠事件，当前展示首项") : QString()));
}
void TimelineWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Home) resetViewport();
    else if (event->key() == Qt::Key_Escape) { selection_.reset(); selecting_ = panning_ = false; update(); }
    else QWidget::keyPressEvent(event);
}
}
