#pragma once
#include "adapters/analysis_export.h"
#include <QObject>
#include <QThread>
#include <QTimer>
namespace gpuview {
class ExportController : public QObject {
    Q_OBJECT
public:
    explicit ExportController(QObject* parent = nullptr) : QObject(parent) {}
    ~ExportController() override;
    bool request(const QString& path, FrameAnalysisPtr analysis, ExportFormat format,
        const QString& notes, const QString& protectedInput = {});
    bool busy() const { return worker_ != nullptr; }
    void cancel();
signals:
    void busyChanged(bool busy);
    void progressChanged(int percent);
    void completed(const QString& path);
    void message(const QString& text);
private:
    QThread* worker_ = nullptr;
    CancelFlag cancel_;
    QTimer poll_;
    std::shared_ptr<std::atomic_int> progress_;
};
}
