/// @file src/ui_widgets/timeline_widget.cpp
/// @brief 自绘多轨时间轴、缩放/平移/选择、轨道滚动及诊断；不可变源数据与交互状态分离。
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
/// 初始化自绘时间轴的输入/焦点策略，源数据由不可变快照拥有。
TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}
/// 绑定新会话并清理旧选择、历史和缓存，重新建立轨道显示映射。
void TimelineWidget::setSnapshot(Snapshot snapshot) {
    snapshot_ = std::move(snapshot);
    firstTrack_ = 0;
    selection_.reset(); selectedEvent_.reset(); history_.clear();
    tracks_.clear();
    if (snapshot_) for (std::uint32_t i = 0; i < snapshot_->tracks.size(); ++i) tracks_.push_back(i);
    cache_.clear();
    syncTrackScroll(0);
    resetViewport(); history_.clear();
}
/// 保存时间视口，重复范围不追加；最多64项避免历史无界增长。
void TimelineWidget::rememberView() {
    if (!history_.empty() && history_.back() == viewport_.range()) return;
    if (history_.size() == 64) history_.erase(history_.begin());
    history_.push_back(viewport_.range());
}
/// 替换可见轨道ID映射并使缓存失效；过滤后行号不等于源轨道ID。
void TimelineWidget::setTracks(std::vector<std::uint32_t> tracks) {
    tracks_.clear();
    if (snapshot_) for (auto id : tracks) if (id < snapshot_->tracks.size() && std::find(tracks_.begin(), tracks_.end(), id) == tracks_.end()) tracks_.push_back(id);
    cache_.clear();
    selectedEvent_.reset(); selection_.reset(); emit selectionCleared();
    syncTrackScroll(0); update();
}
/// 保存旧视图后显示指定范围，发布viewportChanged同步曲线。
void TimelineWidget::showRange(TimeRange range) {
    if (!snapshot_) return;
    rememberView(); viewport_.show(range); emit viewportChanged(viewport_.range().begin, viewport_.range().end); update();
}
/// 存在有效框选时缩放至选区，通过showRange保存返回视图。
void TimelineWidget::zoomSelection() { if (selection_) showRange(*selection_); }
/// 恢复最近保存的时间视口；没有历史时保持当前范围。
void TimelineWidget::previousView() {
    if (history_.empty()) return;
    viewport_.show(history_.back()); history_.pop_back(); emit viewportChanged(viewport_.range().begin, viewport_.range().end); update();
}
/// 高亮事件、滚到对应轨道并发布详情；不改变时间缩放。
void TimelineWidget::selectEvent(const Event& event) {
    const auto it = std::find(tracks_.begin(), tracks_.end(), event.track);
    if (it == tracks_.end()) return;
    setFirstTrack(int(it - tracks_.begin())); selectedEvent_ = event; publishEvent(event);
    update();
}
/// 清除事件选择，设置纳秒选区并缩放，随后发布统计请求。
void TimelineWidget::selectRange(TimeRange range) {
    if(!snapshot_ || range.end<=range.begin) return;
    selectedEvent_.reset(); selection_=range;
    showRange(range); emit rangeSelected(range.begin,range.end); update();
}
/// 选中事件并缩放到其附近，供明细激活和最长事件导航。
void TimelineWidget::focusEvent(const Event& event) {
    if(!snapshot_ || std::find(tracks_.begin(),tracks_.end(),event.track)==tracks_.end()) return;
    selectEvent(event);
    showRange({std::max<TimeNs>(0, event.start - event.duration / 2), event.end() + std::min(event.duration / 2, snapshot_->bounds.end - event.end())});
    update();
}
/// 把纵坐标映射为源轨道ID；超出绘图区或显示映射返回-1。
int TimelineWidget::trackAt(int y) const {
    if (y < top || (y - top) / row >= visibleTrackCount()) return -1;
    const auto index = firstTrack_ + std::uint32_t((y - top) / row);
    return index < tracks_.size() ? int(tracks_[index]) : -1;
}
/// 清除悬停并调用基类，避免鼠标离开后残留高亮。
void TimelineWidget::leaveEvent(QEvent* event) { hover_ = {-1, -1}; update(); QWidget::leaveEvent(event); }
/// 恢复全会话并清除选择，发布范围变化同步其他图表和统计。
void TimelineWidget::resetViewport() {
    rememberView();
    if (snapshot_) { viewport_.reset(snapshot_->bounds); emit viewportChanged(viewport_.range().begin, viewport_.range().end); }
    selection_.reset(); selectedEvent_.reset(); emit selectionCleared();
    update();
}
/// 设置过滤后列表的首个可见行，内部夹紧滚动范围并重绘。
void TimelineWidget::setFirstTrack(int track) {
    syncTrackScroll(track);
    update();
}
/// 按可用高度计算可见行数，至少保留一行以稳定滚动边界。
int TimelineWidget::visibleTrackCount() const {
    return std::max(1, (height() - top) / row);
}
/// 夹紧请求首行并发布滚动参数，统一尺寸变化、过滤和手动滚动的边界处理。
void TimelineWidget::syncTrackScroll(int requestedTrack) {
    const int pageStep = visibleTrackCount();
    const int maximum = std::max(0, int(tracks_.size()) - pageStep);
    // 范围与绘制共享完整可见行数，窗口变高时避免末页留下大量空白。
    firstTrack_ = std::uint32_t(std::clamp(requestedTrack, 0, maximum));
    emit trackScrollChanged(int(firstTrack_), maximum, pageStep);
}
/// 保留Qt尺寸处理并重算可见轨道数，随高度更新滚动范围。
void TimelineWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncTrackScroll(int(firstTrack_));
}
/// GUI线程绘制可见数据与覆盖层，不在绘制回调解析文件或创建逐事件控件。
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
    if (tracks_.empty()) {
        lastPrimitives_ = 0;
        painter.setPen(QColor("#9dafbd"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("无匹配轨道，请调整筛选或展开分组"));
        emit diagnosticsChanged(QStringLiteral("图元 0 | 无匹配轨道"));
        return;
    }
    const int plotWidth = std::max(1, width() - gutter - 12);
    const auto view = viewport_.range();
    const auto visibleTracks = std::uint32_t(visibleTrackCount());
    if (!cacheEnabled_) cache_.clear();
    const auto& batch = cache_.get(snapshot_, {snapshot_->version, view, plotWidth, firstTrack_, tracks_.empty() ? 0u : visibleTracks, tracks_});
    lastPrimitives_ = batch.primitives.size();
    painter.setPen(QColor("#354252"));
    for (int tick = 0; tick <= 8; ++tick) {
        const int x = gutter + plotWidth * tick / 8;
        painter.drawLine(x, top - 5, x, height());
        painter.setPen(QColor("#a9bac8"));
        const auto spanNs = view.end - view.begin;
        const auto time = view.begin + (spanNs / 8) * tick + (spanNs % 8) * tick / 8;
        const auto span = view.end - view.begin;
        const double unit = span >= 10000000000LL ? 1e9 : span >= 1000000 ? 1e6 : 1e3;
        const QString suffix = unit == 1e9 ? " s" : unit == 1e6 ? " ms" : QStringLiteral(" μs");
        const int precision = std::clamp(int(std::ceil(-std::log10(double(span) / unit / 8))), 0, 6);
        const auto label = QString::number(double(time) / unit, 'f', precision) + suffix;
        const auto labelWidth = painter.fontMetrics().horizontalAdvance(label);
        painter.drawText(std::clamp(x - labelWidth / 2, gutter, width() - labelWidth - 4), 23, label);
        painter.setPen(QColor("#273442"));
    }
    for (std::uint32_t index = firstTrack_; index < std::min(std::uint32_t(tracks_.size()), firstTrack_ + visibleTracks); ++index) {
        const auto t = tracks_[index];
        const int y = top + int(index - firstTrack_) * row;
        painter.fillRect(0, y, gutter - 5, row, QColor(t % 2 ? "#182431" : "#15212d"));
        if (selectedEvent_ && selectedEvent_->track == t) painter.fillRect(0, y, width(), row, QColor(250, 190, 80, 35));
        painter.setPen(QColor("#b9cbd9"));
        painter.drawText(QRect(12, y, gutter - 15, row), Qt::AlignVCenter, painter.fontMetrics().elidedText(QString::fromStdString(snapshot_->tracks[t].name), Qt::ElideRight, gutter - 20));
    }
    painter.save();
    painter.setClipRect(gutter, top, plotWidth, height() - top);
    std::vector<int> rows(snapshot_->tracks.size(), -1);
    for (std::size_t i = firstTrack_; i < std::min(tracks_.size(), std::size_t(firstTrack_) + visibleTracks); ++i) rows[tracks_[i]] = int(i - firstTrack_);
    for (const auto& primitive : batch.primitives) {
        const int y = top + rows[primitive.track] * row + 5;
        QColor color = primitive.track < 8 ? QColor("#67a6e8") : QColor("#3cd7b1");
        if (primitive.aggregate) color.setAlpha(std::clamp(55 + int(std::log2(primitive.count + 1) * 30), 55, 240));
        painter.fillRect(QRectF(gutter + primitive.x, y, primitive.width, row - 10), color);
    }
    if (selectedEvent_) {
        const auto& e = *selectedEvent_;
        const auto index = std::find(tracks_.begin(), tracks_.end(), e.track) - tracks_.begin();
        if (index >= firstTrack_ && index < firstTrack_ + visibleTracks) {
            const double scale = plotWidth / double(view.end - view.begin);
            painter.setPen(QPen(QColor("#ffce65"), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(gutter + double(e.start - view.begin) * scale,
                top + int(index - firstTrack_) * row + 4, std::max(2.0, double(e.duration) * scale), row - 8));
        }
    }
    if (hover_.x() >= gutter && hover_.y() >= top) {
        painter.setPen(QPen(QColor("#c9d9e5"), 1, Qt::DashLine));
        painter.drawLine(hover_.x(), top, hover_.x(), height());
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
/// 以gutter为原点把横坐标映射为视口内纳秒时刻。
TimeNs TimelineWidget::timeAt(double x) const {
    const auto view = viewport_.range();
    const double fraction = std::clamp((x - gutter) / std::max(1, width() - gutter - 12), 0.0, 1.0);
    return view.begin + TimeNs(fraction * double(view.end - view.begin));
}
/// 名称区滚轮滚轨道，事件区按鼠标锚点缩放时间，两者均受视口边界约束。
void TimelineWidget::wheelEvent(QWheelEvent* event) {
    if (!snapshot_) return;
    if (event->position().x() < gutter) {
        setFirstTrack(int(firstTrack_) - event->angleDelta().y() / 120 * 3); event->accept(); return;
    }
    rememberView();
    const double anchor = (event->position().x() - gutter) / std::max(1, width() - gutter - 12);
    viewport_.zoom(std::pow(1.25, event->angleDelta().y() / 120.0), anchor);
    emit viewportChanged(viewport_.range().begin, viewport_.range().end);
    update();
    event->accept();
}
/// 根据按键和命中开始选择/平移或发布事件；输入坐标为逻辑像素。
void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (!snapshot_ || event->position().x() < gutter || event->position().y() < top) return;
    setFocus();
    press_ = previous_ = event->position().toPoint();
    panning_ = event->button() == Qt::MiddleButton;
    if (panning_) rememberView();
    selecting_ = event->button() == Qt::LeftButton;
    if (selecting_) { selection_.reset(); selectedEvent_.reset(); emit selectionCleared(); }
}
/// 拖动时更新交互，空闲时更新悬停；不改写源事件。
void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!snapshot_) return;
    const auto point = event->position().toPoint();
    hover_ = point;
    if (!panning_ && !selecting_) {
        const int track = trackAt(point.y());
        if (track >= 0 && point.x() < gutter) QToolTip::showText(event->globalPosition().toPoint(), QString::fromStdString(snapshot_->tracks[track].name), this);
        else if (track >= 0) {
            const auto time = timeAt(point.x());
            const auto bucket = std::min<std::size_t>(std::size_t(time / snapshot_->bucketWidth), snapshot_->tracks[track].overviewCounts.size() - 1);
            QToolTip::showText(event->globalPosition().toPoint(), QStringLiteral("时间 %1 ms\n概览桶相交事件 %2（近似范围，非利用率）")
                .arg(double(time) / 1e6, 0, 'f', 6).arg(snapshot_->tracks[track].overviewCounts[bucket]), this);
        }
        update();
    }
    if (panning_) {
        viewport_.pan(double(previous_.x() - point.x()) / std::max(1, width() - gutter - 12));
        emit viewportChanged(viewport_.range().begin, viewport_.range().end);
        previous_ = point;
        update();
    } else if (selecting_) {
        const auto a = timeAt(press_.x()), b = timeAt(point.x());
        selection_ = TimeRange{std::min(a, b), std::max(a, b)};
        update();
    }
}
/// 结束拖动并提交框选；短点击精确拾取，避免误触范围统计。
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
/// 按点击时间和轨道查询原始事件，不把LOD聚合图元当作事件身份。
void TimelineWidget::pick(const QPoint& point) {
    if (point.y() < top) return;
    const int track = trackAt(point.y());
    if (track < 0) return;
    const auto time = timeAt(point.x());
    const auto query = snapshot_->tracks[track].index.query({time, time + 1}, 1);
    if (query.events.empty()) { emit eventPicked(0, QStringLiteral("该位置没有精确事件；概览桶不能替代原始事件")); return; }
    const auto& e = *query.events.front();
    selectedEvent_ = e;
    publishEvent(e);
    if (query.truncated) QToolTip::showText(mapToGlobal(point), QStringLiteral("该位置有重叠事件，当前选中首项"), this);
}
/// 把源事件与来源语义转为详情并发布，避免误把帧宽度当Kernel时长。
void TimelineWidget::publishEvent(const Event& e) {
    emit eventPicked(e.id, QStringLiteral("记录 #%1\n名称：%2\n轨道：%3\n开始：%4 ms\n时长：%5 ms\n矩形结束：%6 ms\n来源：%7\n%8")
        .arg(e.id).arg(QString::fromStdString(snapshot_->names[e.name])).arg(QString::fromStdString(snapshot_->tracks[e.track].name))
        .arg(double(e.start) / 1e6, 0, 'f', 6).arg(double(e.duration) / 1e6, 0, 'f', 6)
        .arg(double(e.end()) / 1e6, 0, 'f', 6).arg(QString::fromStdString(snapshot_->source))
        .arg(snapshot_->frames ? QStringLiteral("开始为归一化Present时刻；时长为前一Present间隔；矩形结束仅为可视化编码，不是GPU结束时间。") : QStringLiteral("教学模拟事件，不是真实GPU采集")));
}

/// 处理Home全览和Esc清除，其余按键交给QWidget。
void TimelineWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Home) resetViewport();
    else if (event->key() == Qt::Key_Escape) { selection_.reset(); selectedEvent_.reset(); selecting_ = panning_ = false; emit selectionCleared(); update(); }
    else QWidget::keyPressEvent(event);
}
}
