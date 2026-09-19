#include "ui_widgets/frame_table_model.h"
#include <QColor>
namespace gpuview {
void FrameTableModel::setAnalysis(FrameAnalysisPtr analysis) {
    beginResetModel(); analysis_=std::move(analysis); endResetModel();
}
int FrameTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || !analysis_ ? 0 : int(analysis_->rows.size());
}
const Event* FrameTableModel::eventAt(int row) const {
    return row>=0 && row<rowCount() ? &analysis_->event(std::size_t(row)) : nullptr;
}
QVariant FrameTableModel::data(const QModelIndex& index, int role) const {
    const auto* e=eventAt(index.row()); if(!index.isValid() || !e || index.column()<0 || index.column()>=4) return {};
    const bool longFrame=analysis_->rows[std::size_t(index.row())].longFrame;
    if(role==Qt::UserRole) return QVariant::fromValue(qulonglong(e->id));
    if(role==Qt::ForegroundRole && longFrame) return QColor("#ffce65");
    if(role==Qt::TextAlignmentRole) return int(Qt::AlignRight|Qt::AlignVCenter);
    if(role!=Qt::DisplayRole) return {};
    switch(index.column()) {
    case 0: return QVariant::fromValue(qulonglong(e->id));
    case 1: return QString::number(double(e->start)/1e6,'f',6);
    case 2: return QString::number(double(e->duration)/1e6,'f',6);
    default: return longFrame ? QStringLiteral("是") : QStringLiteral("否");
    }
}
QVariant FrameTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role!=Qt::DisplayRole) return {};
    if(orientation==Qt::Vertical) return section+1;
    const QStringList labels{QStringLiteral("记录 ID"),QStringLiteral("Present ms"),QStringLiteral("帧间隔 ms"),QStringLiteral("长帧")};
    return section>=0 && section<labels.size() ? labels[section] : QVariant();
}
void FrameTableModel::sort(int column, Qt::SortOrder order) {
    // 不在GUI线程对百万QVariant做排序；控制器以最新请求模式后台重建行索引。
    if(column>=0 && column<4) emit sortRequested(column,order==Qt::DescendingOrder);
}
}
