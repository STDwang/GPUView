#include "application/session_controller.h"
#include "adapters/synthetic_source.h"
#include "adapters/presentmon_csv.h"
#include <QElapsedTimer>
namespace gpuview {
SessionController::SessionController(QObject* parent) : QObject(parent) {
    poll_.setInterval(100);
    connect(&poll_, &QTimer::timeout, this, [this] {
        const auto value = progress_ ? progress_->percent.load(std::memory_order_relaxed) : 0;
        if (value != lastProgress_) { lastProgress_ = value; emit progressChanged(value); }
    });
}
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
void SessionController::requestSynthetic(std::size_t count) {
    Request request{count, ++generation_, {}};
    if (worker_) {
        pending_.replace(request);
        cancel_->store(true, std::memory_order_relaxed);
    } else start(request);
}
void SessionController::requestFile(const QString& path) {
    Request request{0, ++generation_, path};
    if (worker_) { pending_.replace(request); cancel_->store(true, std::memory_order_relaxed); }
    else start(request);
}
void SessionController::cancel() {
    ++generation_; // 即使旧任务刚好完成，迟到结果也不能覆盖现有会话。
    pending_.take();
    if (cancel_) cancel_->store(true, std::memory_order_relaxed);
}
void SessionController::start(Request request) {
    cancel_ = std::make_shared<std::atomic_bool>(false);
    progress_ = std::make_shared<ProgressMailbox>();
    auto result = std::make_shared<Result>();
    const auto cancel = cancel_;
    const auto progress = progress_;
    // 捕获值不捕获窗口/控制器this，worker只生产数据，不访问任何GUI对象。
    worker_ = QThread::create([request, cancel, progress, result] {
        QElapsedTimer timer; timer.start();
        try {
            auto report = [progress](int p) { progress->percent.store(p, std::memory_order_relaxed); };
            result->snapshot = request.path.isEmpty() ? generateTrace(request.count, request.generation, cancel, report)
                : loadPresentMon(std::filesystem::path(request.path.toStdWString()), request.generation, cancel, report);
            result->loadMs = timer.nsecsElapsed() / 1e6;
        } catch (const Cancelled&) { result->cancelled = true;
        } catch (const std::exception& e) { result->error = QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this); // finished后的deleteLater尚未处理时，由控制器兜底释放。
    auto* thread = worker_;
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
