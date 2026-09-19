#pragma once
#include "ui_widgets/frame_table_model.h"
#include "adapters/analysis_export.h"
#include <QWidget>
class QTableView; class QLabel; class QLineEdit; class QPushButton;
namespace gpuview {
class FrameDetailsPanel : public QWidget {
    Q_OBJECT
public:
    explicit FrameDetailsPanel(QWidget* parent = nullptr);
    void setAnalysis(FrameAnalysisPtr analysis);
    void selectId(std::uint64_t id);
    void resetSession();
    void setExportBusy(bool busy);
    void setExportProgress(int percent);
    FrameAnalysisPtr analysis() const { return model_->analysis(); }
signals:
    void eventSelected(const gpuview::Event& event);
    void eventActivated(const gpuview::Event& event);
    void sortRequested(int column, bool descending);
    void exportRequested(gpuview::ExportFormat format, const QString& notes);
    void cancelExport();
private:
    FrameTableModel* model_;
    QTableView* table_;
    QLabel* label_;
    QLineEdit* notes_;
    QPushButton* exportButton_;
    QPushButton* cancelButton_;
    std::optional<std::uint64_t> selectedId_;
    bool exporting_ = false;
};
}
