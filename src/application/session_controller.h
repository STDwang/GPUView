#pragma once
#include "core/trace_store.h"
#include <QObject>
#include <QThread>
#include <QTimer>

namespace gpuview {
// GUI只在固定频率读取一个原子进度值，生产速度再高也不会逐条排入事件队列。
struct ProgressMailbox { std::atomic_int percent{0}; };
class SessionController : public QObject {
    Q_OBJECT
public:
    explicit SessionController(QObject* parent = nullptr);
    ~SessionController() override;
    void requestSynthetic(std::size_t count);
    void requestFile(const QString& path);
    void cancel();
    double loadMs() const { return loadMs_; }
    bool busy() const { return worker_ != nullptr; }
    Snapshot snapshot() const { return current_; }
    std::size_t pendingRequests() const { return pending_.hasValue() ? 1 : 0; }
signals:
    void snapshotReady();
    void busyChanged(bool busy);
    void progressChanged(int percent);
    void message(const QString& text);
private:
    struct Request { std::size_t count; std::uint64_t generation; QString path; };
    struct Result { Snapshot snapshot; QString error; bool cancelled = false; double loadMs = 0; };
    void start(Request request);
    QThread* worker_ = nullptr;
    QTimer poll_;
    CancelFlag cancel_;
    std::shared_ptr<ProgressMailbox> progress_;
    LatestRequest<Request> pending_;
    Snapshot current_;
    std::uint64_t generation_ = 0;
    int lastProgress_ = -1;
    double loadMs_ = 0;
};
}
