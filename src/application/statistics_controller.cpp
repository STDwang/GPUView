#include "application/statistics_controller.h"
namespace gpuview {
StatisticsController::~StatisticsController() {
    cancel();
    if (worker_) { disconnect(worker_, nullptr, this, nullptr); worker_->wait(); delete worker_; }
}
void StatisticsController::cancel() {
    ++generation_; pending_.take();
    if (cancel_) cancel_->store(true, std::memory_order_relaxed);
}
void StatisticsController::request(Snapshot snapshot, TimeRange range, std::vector<std::uint32_t> tracks) {
    Request request{std::move(snapshot), range, std::move(tracks), ++generation_};
    if (worker_) { pending_.replace(std::move(request)); cancel_->store(true, std::memory_order_relaxed); }
    else start(std::move(request));
}
void StatisticsController::start(Request request) {
    cancel_ = std::make_shared<std::atomic_bool>(false);
    auto value = std::make_shared<Statistics>();
    auto error = std::make_shared<QString>();
    const auto cancel = cancel_;
    worker_ = QThread::create([request, cancel, value, error] {
        try { *value = calculateStatistics(*request.snapshot, request.range, request.tracks, cancel); }
        catch (const Cancelled&) {}
        catch (const std::exception& e) { *error = QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this);
    auto* thread = worker_;
    connect(thread, &QThread::finished, this, [this, thread, value, error, generation = request.generation] {
        thread->wait(); worker_ = nullptr; thread->deleteLater();
        // 过滤/框选/换文件都会推进代次；取消后的迟到统计不得刷新侧栏。
        if (generation == generation_) {
            if (error->isEmpty()) { result_ = *value; emit ready(); }
            else emit failed(*error);
        }
        if (auto next = pending_.take()) start(std::move(*next));
    }, Qt::QueuedConnection);
    worker_->start();
}
}
