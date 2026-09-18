#pragma once
#include "core/statistics.h"
#include <QObject>
#include <QThread>
namespace gpuview {
class StatisticsController : public QObject {
    Q_OBJECT
public:
    explicit StatisticsController(QObject* parent = nullptr) : QObject(parent) {}
    ~StatisticsController() override;
    void request(Snapshot snapshot, TimeRange range, std::vector<std::uint32_t> tracks);
    void cancel();
    const Statistics& result() const { return result_; }
signals:
    void ready();
    void failed(const QString& error);
private:
    struct Request { Snapshot snapshot; TimeRange range; std::vector<std::uint32_t> tracks; std::uint64_t generation; };
    void start(Request request);
    QThread* worker_ = nullptr;
    CancelFlag cancel_;
    LatestRequest<Request> pending_;
    std::uint64_t generation_ = 0;
    Statistics result_;
};
}
