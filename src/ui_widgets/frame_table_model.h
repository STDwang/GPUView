#pragma once
#include "core/frame_analysis.h"
#include <QAbstractTableModel>
namespace gpuview {
class FrameTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit FrameTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setAnalysis(FrameAnalysisPtr analysis);
    FrameAnalysisPtr analysis() const { return analysis_; }
    const Event* eventAt(int row) const;
    int rowForId(std::uint64_t id) const { return analysis_ ? analysis_->rowForId(id) : -1; }
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 4; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void sort(int column, Qt::SortOrder order) override;
signals:
    void sortRequested(int column, bool descending);
private:
    FrameAnalysisPtr analysis_;
};
}
