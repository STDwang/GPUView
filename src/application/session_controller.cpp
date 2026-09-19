/// @file src/application/session_controller.cpp
/// @brief 会话加载协调器；只保留最新待办，GUI发布只读快照，失败或取消保留旧会话。
#include "application/session_controller.h"
#include "adapters/synthetic_source.h"
#include "adapters/presentmon_csv.h"
#include <QElapsedTimer>
namespace gpuview {
/// 建立GUI线程的进度定时器，加载Worker通过原子邮箱交付进度。
SessionController::SessionController(QObject* parent) : QObject(parent) {
    poll_.setInterval(100);
    // GUI轮询进度且只在值变化时发信号，避免重复刷新。
    connect(&poll_, &QTimer::timeout, this, [this] {
        const auto value = progress_ ? progress_->percent.load(std::memory_order_relaxed) : 0;
        if (value != lastProgress_) { lastProgress_ = value; emit progressChanged(value); }
    });
}
/// 停轮询、取消并等待加载线程；析构完成后不遗留访问本对象的Worker。
SessionController::~SessionController() {
    poll_.stop();
    if (cancel_) cancel_->store(true, std::memory_order_relaxed);
    if (worker_) {
        // Q06：先停止生产者再销毁接收者；生成/排序/索引循环均有协作取消点。
        disconnect(worker_, nullptr, this, nullptr);
        worker_->wait();
        delete worker_;
    }
}
/// 请求生成count条教学事件；推进代次，运行期间只保留最新一个待办。
void SessionController::requestSynthetic(std::size_t count) {
    Request request{count, ++generation_, {}};
    if (worker_) {
        pending_.replace(request);
        cancel_->store(true, std::memory_order_relaxed);
    } else start(request);
}
/// 后台读取path；旧会话保留到新快照成功发布，失败或取消不清空分析。
void SessionController::requestFile(const QString& path) {
    Request request{0, ++generation_, path};
    if (worker_) { pending_.replace(request); cancel_->store(true, std::memory_order_relaxed); }
    else start(request);
}
/// 推进代次、清空待办并置位取消，使迟到结果失效；已发布会话继续保留。
void SessionController::cancel() {
    ++generation_; // 即使旧任务刚好完成，迟到结果也不能覆盖现有会话。
    pending_.take();
    if (cancel_) cancel_->store(true, std::memory_order_relaxed);
}
/// 启动加载Worker并连接GUI完成回调；只有当前代次成功结果才替换会话和来源路径。
void SessionController::start(Request request) {
    cancel_ = std::make_shared<std::atomic_bool>(false);
    progress_ = std::make_shared<ProgressMailbox>();
    auto result = std::make_shared<Result>();
    const auto cancel = cancel_;
    // 按值持有本次任务邮箱，后续新请求替换成员progress_不影响当前Worker。
    const auto progress = progress_;
    // 捕获值不捕获窗口/控制器this，worker只生产数据，不访问任何GUI对象。
    // Worker持有冻结请求与共享结果/进度，加载完成前不改写current_。
    worker_ = QThread::create([request, cancel, progress, result] {
        QElapsedTimer timer; timer.start();
        try {
            // Worker进度回调只覆盖原子邮箱，不能在此直接操作GUI或逐记录发信号。
            auto report = [progress](int p) { progress->percent.store(p, std::memory_order_relaxed); };
            result->snapshot = request.path.isEmpty() ? generateTrace(request.count, request.generation, cancel, report)
                : loadPresentMon(std::filesystem::path(request.path.toStdWString()), request.generation, cancel, report);
            result->loadMs = timer.nsecsElapsed() / 1e6;
        } catch (const Cancelled&) { result->cancelled = true;
        } catch (const std::exception& e) { result->error = QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this); // finished后的deleteLater尚未处理时，由控制器兜底释放。
    auto* thread = worker_;
    // GUI完成回调等待线程后读取结果，只发布当前有效代次；失败和取消保留旧会话。
    connect(thread, &QThread::finished, this, [this, thread, request, result] {
        // wait建立完成同步；只有GUI线程在此发布快照和发模型通知。
        thread->wait();
        worker_ = nullptr;
        thread->deleteLater();
        poll_.stop();
        if (request.generation == generation_) {
            if (result->snapshot) { current_ = result->snapshot; sourcePath_ = request.path; loadMs_ = result->loadMs; emit snapshotReady(); }
            else if (!result->error.isEmpty()) emit message(QStringLiteral("加载失败：%1（保留原会话）").arg(result->error));
        }
        if (auto next = pending_.take()) start(*next);
        else { emit busyChanged(false); if (result->cancelled) emit message(QStringLiteral("已取消，保留原会话")); }
    }, Qt::QueuedConnection);
    lastProgress_ = -1;
    emit busyChanged(true);
    poll_.start();
    thread->start();
}
}
