#pragma once
#include "core/frame_analysis.h"
#include <QObject>
#include <QThread>
namespace gpuview {
class StatisticsController : public QObject {
    Q_OBJECT
public:
    explicit StatisticsController(QObject* parent = nullptr) : QObject(parent) {}
    ~StatisticsController() override;
    void request(Snapshot snapshot, TimeRange range, std::vector<std::uint32_t> tracks, FrameSort sort = FrameSort::Start, bool descending = false);
    void cancel();
    FrameAnalysisPtr frameDetails() const { return frameDetails_; }
    const Statistics& result() const { return result_; }
signals:
    void ready();
    void failed(const QString& error);
private:
    struct Request { Snapshot snapshot; TimeRange range; std::vector<std::uint32_t> tracks; std::uint64_t generation; FrameSort sort; bool descending; };
    void start(Request request);
    QThread* worker_ = nullptr;
    CancelFlag cancel_;
    LatestRequest<Request> pending_;
    std::uint64_t generation_ = 0;
    Statistics result_;
    FrameAnalysisPtr frameDetails_;
};
}
