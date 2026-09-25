/// @file event_analysis_controller.h
/// @brief 有界事件分析任务协调器，GUI串行请求、Worker计算、代次校验后发布。
#pragma once
#include "core/event_analysis.h"
#include <QObject>
#include <QThread>
namespace gpuview {
/// 一个运行任务加一个最新待办，快速搜索/排序不会创建无界线程。
class EventAnalysisController : public QObject {
    Q_OBJECT
public:
    /// 在GUI线程创建协调器，parent管理QObject寿命。
    explicit EventAnalysisController(QObject* parent=nullptr):QObject(parent) {}
    /// 取消并等待Worker，禁止窗口退出后继续访问结果容器。
    ~EventAnalysisController() override;
    /// 冻结参数并替换待办，旧代次不可发布。
    void request(Snapshot source, EventFilter filter);
    /// 丢弃待办并取消当前任务，不清理调用方拥有的快照。
    void cancel();
    /// 返回最近成功发布的只读结果。
    EventAnalysisPtr result() const { return result_; }
signals:
    /// GUI线程通知当前结果已就绪。
    void ready();
    /// GUI线程发布当前请求错误，不发布迟到错误。
    void failed(const QString& message);
private:
    /// 待执行请求所有参数，不引用UI对象。
    struct Request {
        /// 本次源快照所有权。
        Snapshot source;
        /// 冻结的筛选/排序条件。
        EventFilter filter;
        /// 发起代次，用于拒绝迟到完成通知。
        std::uint64_t generation;
    };
    /// 建立Worker及队列完成回调，只有GUI可以发布result_。
    void start(Request request);
    /// 当前Worker，完成后deleteLater，析构时wait兜底。
    QThread* worker_=nullptr;
    /// 本次共享协作取消令牌。
    CancelFlag cancel_;
    /// 串行GUI使用的单项待办邮箱。
    LatestRequest<Request> pending_;
    /// 当前有效请求代次。
    std::uint64_t generation_=0;
    /// 最近成功发布的不可变结果。
    EventAnalysisPtr result_;
};
}
