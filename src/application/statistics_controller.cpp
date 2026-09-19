/// @file src/application/statistics_controller.cpp
/// @brief 统计/帧排序任务协调器；后台计算，代次检查过滤迟到结果，退出时等待Worker。
#include "application/statistics_controller.h"
namespace gpuview {
/// 取消当前统计并等待Worker，保证退出后任务不再访问成员状态。
StatisticsController::~StatisticsController() {
    cancel();
    if (worker_) { disconnect(worker_, nullptr, this, nullptr); worker_->wait(); delete worker_; }
}
/// 推进统计代次、清空待办并取消运行计算；旧任务即使完成也不能发布。
void StatisticsController::cancel() {
    ++generation_; pending_.take();
    if (cancel_) cancel_->store(true, std::memory_order_relaxed);
}
/// 冻结快照、范围、轨道及排序参数；运行期间仅替换最新待办并取消旧计算。
void StatisticsController::request(Snapshot snapshot, TimeRange range, std::vector<std::uint32_t> tracks, FrameSort sort, bool descending) {
    Request request{std::move(snapshot), range, std::move(tracks), ++generation_, sort, descending};
    if (worker_) { pending_.replace(std::move(request)); cancel_->store(true, std::memory_order_relaxed); }
    else start(std::move(request));
}
/// Worker执行Trace统计或帧分析；GUI完成回调检查代次、发布结果并启动最新待办。
void StatisticsController::start(Request request) {
    cancel_ = std::make_shared<std::atomic_bool>(false);
    auto value = std::make_shared<Statistics>();
    auto details = std::make_shared<FrameAnalysisPtr>();
    auto error = std::make_shared<QString>();
    const auto cancel = cancel_;
    // 冻结请求按值交给Worker；结果先写独立容器，GUI尚不可见，取消也不发布半成品。
    worker_ = QThread::create([request, cancel, value, error, details] {
        try {
            if(request.snapshot->frames) {
                *details=analyzeFrames(request.snapshot,request.range,request.tracks,request.sort,request.descending,cancel);
                *value=(*details)->summary;
            } else *value = calculateStatistics(*request.snapshot, request.range, request.tracks, cancel);
        }
        catch (const Cancelled&) {}
        catch (const std::exception& e) { *error = QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this);
    auto* thread = worker_;
    // GUI完成回调以generation判断是否迟到；仅当前代次发布，随后运行最新待办。
    connect(thread, &QThread::finished, this, [this, thread, value, error, details, generation = request.generation] {
        thread->wait(); worker_ = nullptr; thread->deleteLater();
        // 过滤/框选/换文件都会推进代次；取消后的迟到统计不得刷新侧栏。
        if (generation == generation_) {
            if (error->isEmpty()) { result_ = std::move(*value); frameDetails_ = *details; emit ready(); }
            else emit failed(*error);
        }
        if (auto next = pending_.take()) start(std::move(*next));
    }, Qt::QueuedConnection);
    worker_->start();
}
}
