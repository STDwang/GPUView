/// @file src/ui_widgets/timeline_widget.h
/// @brief 自绘多轨时间轴、缩放/平移/选择、轨道滚动及诊断；不可变源数据与交互状态分离。
#pragma once
#include "core/render_query.h"
#include <QWidget>
#include <QPoint>
namespace gpuview {
/// 有界几何驱动的自绘时间轴；界面交互与原始数据生命周期独立。
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    /// 初始化自绘时间轴的输入/焦点策略，源数据由不可变快照拥有。
    explicit TimelineWidget(QWidget* parent = nullptr);
    /// 绑定新会话并清理旧选择、历史和缓存，重新建立轨道显示映射。
    void setSnapshot(Snapshot snapshot);
    /// 恢复全会话并清除选择，发布范围变化同步其他图表和统计。
    void resetViewport();
    /// 设置过滤后列表的首个可见行，内部夹紧滚动范围并重绘。
    void setFirstTrack(int track);
    /// 替换可见轨道ID映射并使缓存失效；过滤后行号不等于源轨道ID。
    void setTracks(std::vector<std::uint32_t> tracks);
    /// 只读借用有序源轨道ID列表，用于统计当前显示的数据集合。
    const std::vector<std::uint32_t>& tracks() const { return tracks_; }
    /// 存在有效框选时缩放至选区，通过showRange保存返回视图。
    void zoomSelection();
    /// 恢复最近保存的时间视口；没有历史时保持当前范围。
    void previousView();
    /// 高亮事件、滚到对应轨道并发布详情；不改变时间缩放。
    void selectEvent(const Event& event);
    /// 清除事件选择，设置纳秒选区并缩放，随后发布统计请求。
    void selectRange(TimeRange range);
    /// 选中事件并缩放到其附近，供明细激活和最长事件导航。
    void focusEvent(const Event& event);
    /// 保存旧视图后显示指定范围，发布viewportChanged同步曲线。
    void showRange(TimeRange range);
    /// 返回选中事件的值副本；未选择时为空，不借用临时查询指针。
    std::optional<Event> selectedEvent() const { return selectedEvent_; }
    /// 切换几何缓存并清空旧项，用于同条件消融测量，不改变查询语义。
    void setCacheEnabled(bool enabled) { cacheEnabled_ = enabled; cache_.clear(); }
    /// 返回当前半开纳秒视口，供图表与交互测试同步。
    TimeRange visibleRange() const { return viewport_.range(); }
    /// 返回最近绘制图元数，概览图元可能对应多个事件。
    std::size_t primitiveCount() const { return lastPrimitives_; }
    /// 返回最近CPU绘制耗时（毫秒），不含显示合成或屏幕呈现延迟。
    double lastPaintMs() const { return lastPaintMs_; }
signals:
    /// 发布稳定ID与详情文本，供GUI线程内的表格和侧栏联动。
    void eventPicked(qulonglong id, const QString& details);
    /// 发布框选半开纳秒范围，触发后台精确统计。
    void rangeSelected(qint64 begin, qint64 end);
    /// 发布绘制耗时、图元和缓存诊断文本供状态栏显示。
    void diagnosticsChanged(const QString& text);
    /// 发布可见纳秒范围，供帧曲线共享横坐标。
    void viewportChanged(qint64 begin, qint64 end);
    /// 通知上层移除局部选区，恢复全会话/当前组统计。
    void selectionCleared();
    /// 发布首行、最大首行和页步长，使外部滚动条与可见轨道一致。
    void trackScrollChanged(int firstTrack, int maximum, int pageStep);
protected:
    /// 清除悬停并调用基类，避免鼠标离开后残留高亮。
    void leaveEvent(QEvent*) override;
    /// 保留Qt尺寸处理并重算可见轨道数，随高度更新滚动范围。
    void resizeEvent(QResizeEvent*) override;
    /// GUI线程绘制可见数据与覆盖层，不在绘制回调解析文件或创建逐事件控件。
    void paintEvent(QPaintEvent*) override;
    /// 名称区滚轮滚轨道，事件区按鼠标锚点缩放时间，两者均受视口边界约束。
    void wheelEvent(QWheelEvent*) override;
    /// 根据按键和命中开始选择/平移或发布事件；输入坐标为逻辑像素。
    void mousePressEvent(QMouseEvent*) override;
    /// 拖动时更新交互，空闲时更新悬停；不改写源事件。
    void mouseMoveEvent(QMouseEvent*) override;
    /// 结束拖动并提交框选；短点击精确拾取，避免误触范围统计。
    void mouseReleaseEvent(QMouseEvent*) override;
    /// 处理Home全览和Esc清除，其余按键交给QWidget。
    void keyPressEvent(QKeyEvent*) override;
private:
    /// 按可用高度计算可见行数，至少保留一行以稳定滚动边界。
    int visibleTrackCount() const;
    /// 夹紧请求首行并发布滚动参数，统一尺寸变化、过滤和手动滚动的边界处理。
    void syncTrackScroll(int requestedTrack);
    /// 把源事件与来源语义转为详情并发布，避免误把帧宽度当Kernel时长。
    void publishEvent(const Event& event);
    /// 保存时间视口，重复范围不追加；最多64项避免历史无界增长。
    void rememberView();
    /// 把纵坐标映射为源轨道ID；超出绘图区或显示映射返回-1。
    int trackAt(int y) const;
    /// 以gutter为原点把横坐标映射为视口内纳秒时刻。
    TimeNs timeAt(double x) const;
    /// 按点击时间和轨道查询原始事件，不把LOD聚合图元当作事件身份。
    void pick(const QPoint& point);
    /// 轨道名称区宽度（逻辑像素），与帧图/热力图绘图区原点一致。
    static constexpr int gutter = 150;
    /// 顶部刻度区高度（逻辑像素），轨道从此处开始绘制。
    static constexpr int top = 40;
    /// 轨道行高（逻辑像素），用于裁剪、滚动和鼠标命中。
    static constexpr int row = 32;
    /// 当前只读快照，持有期间源事件及索引保持有效。
    Snapshot snapshot_;
    /// 过滤后的源轨道ID列表，可见行号通过它映射回源轨道。
    std::vector<std::uint32_t> tracks_;
    /// 最多64项视口历史，供上一视图恢复。
    std::vector<TimeRange> history_;
    /// 选中事件值副本，换快照或清除选择时重置。
    std::optional<Event> selectedEvent_;
    /// 悬停逻辑像素位置，负坐标表示鼠标已离开。
    QPoint hover_{-1, -1};
    /// 几何缓存开关，用于性能消融对照。
    bool cacheEnabled_ = true;
    /// 统一处理时间缩放、平移和边界夹紧的视口模型。
    TimeViewport viewport_;
    /// 单项纯几何缓存，颜色/字体在绘制层处理。
    RenderCache cache_;
    /// 过滤列表中的首个可见行序号，不是源轨道ID。
    std::uint32_t firstTrack_ = 0;
    /// 鼠标按下位置，用于区分点击与拖动。
    QPoint press_;
    /// 上一鼠标移动位置，用于计算增量平移。
    QPoint previous_;
    /// 是否正用中键拖动平移时间轴。
    bool panning_ = false;
    /// 是否正用左键拖动选择时间范围。
    bool selecting_ = false;
    /// 用户框选的半开纳秒范围，与单事件选择分开保存。
    std::optional<TimeRange> selection_;
    /// 最近绘制图元数，用于验证LOD工作量有界。
    std::size_t lastPrimitives_ = 0;
    /// 最近CPU绘制耗时（毫秒），不代表屏幕帧间隔。
    double lastPaintMs_ = 0;
};
}
