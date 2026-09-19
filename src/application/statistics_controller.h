/// @file src/application/statistics_controller.h
/// @brief 统计/帧排序任务协调器；后台计算，代次检查过滤迟到结果，退出时等待Worker。
#pragma once
#include "core/frame_analysis.h"
#include <QObject>
#include <QThread>
namespace gpuview {
/// 后台统计协调器；按请求代次发布只读结果，不允许旧选区结果覆盖新选区。
class StatisticsController : public QObject {
    Q_OBJECT
public:
    /// 创建GUI线程的统计协调器；计算与排序由独立Worker执行。
    explicit StatisticsController(QObject* parent = nullptr) : QObject(parent) {}
    /// 取消当前统计并等待Worker，保证退出后任务不再访问成员状态。
    ~StatisticsController() override;
    /// 冻结快照、范围、轨道及排序参数；运行期间仅替换最新待办并取消旧计算。
    void request(Snapshot snapshot, TimeRange range, std::vector<std::uint32_t> tracks, FrameSort sort = FrameSort::Start, bool descending = false);
    /// 推进统计代次、清空待办并取消运行计算；旧任务即使完成也不能发布。
    void cancel();
    /// 取得最近发布的帧分析快照；教学Trace对应空指针，寿命由shared_ptr管理。
    FrameAnalysisPtr frameDetails() const { return frameDetails_; }
    /// 只读借用最近统计；仅GUI线程访问，下一次结果发布会替换其内容。
    const Statistics& result() const { return result_; }
signals:
    /// 通知GUI当前代次统计已发布，可同时读取result与frameDetails。
    void ready();
    /// 发布当前统计请求的错误文本；迟到请求不得覆盖新界面状态。
    void failed(const QString& error);
private:
    /// 一次异步任务的冻结参数，代次用于完成时判断结果是否仍有效。
    struct Request {
        /// 任务持有的共享只读快照，没有跨线程写入口。
        Snapshot snapshot;
        /// 本次统计/几何查询的半开纳秒范围。
        TimeRange range;
        /// 参与分析的源轨道ID集合，不能用过滤后行号代替。
        std::vector<std::uint32_t> tracks;
        /// 发起请求时的代次，完成时用于丢弃迟到结果。
        std::uint64_t generation;
        /// 当前排序字段，枚举顺序与表格四列一致。
        FrameSort sort;
        /// 主键是否降序；同值仍以稳定ID升序确定顺序。
        bool descending;
    };
    /// Worker执行Trace统计或帧分析；GUI完成回调检查代次、发布结果并启动最新待办。
    void start(Request request);
    /// 当前Worker，协调器负责退出等待，完成后在GUI线程释放。
    QThread* worker_ = nullptr;
    /// 共享原子取消标志，仅请求协作退出，不强制终止线程。
    CancelFlag cancel_;
    /// 唯一待执行请求，新请求替换旧待办以避免无界排队。
    LatestRequest<Request> pending_;
    /// 当前请求代次，递增使所有旧任务完成结果失效。
    std::uint64_t generation_ = 0;
    /// 最近成功发布的统计，仅由GUI线程完成回调替换。
    Statistics result_;
    /// 与统计同步发布的帧分析快照，非帧结果为空。
    FrameAnalysisPtr frameDetails_;
};
}
