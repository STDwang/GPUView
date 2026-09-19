/// @file src/ui_widgets/frame_table_model.cpp
/// @brief 惰性只读Model/View适配器；不创建逐帧控件，排序交给后台，源数据由快照拥有。
#include "ui_widgets/frame_table_model.h"
#include <QColor>
namespace gpuview {
/// GUI线程绑定只读分析并刷新显示；空值表示当前分析未就绪，不继续展示旧范围数据。
void FrameTableModel::setAnalysis(FrameAnalysisPtr analysis) {
    beginResetModel(); analysis_=std::move(analysis); endResetModel();
}
/// 返回完整帧行数；平面模型不提供子节点，非空parent返回0。
int FrameTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || !analysis_ ? 0 : int(analysis_->rows.size());
}
/// 借用行对应源事件，越界返回nullptr；不能在其快照释放后继续使用。
const Event* FrameTableModel::eventAt(int row) const {
    return row>=0 && row<rowCount() ? &analysis_->event(std::size_t(row)) : nullptr;
}
/// 按Qt角色惰性生成单元格，DisplayRole格式化毫秒，UserRole保留稳定ID。
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
/// 提供列名和行序号；行序号只是位置，不能跨排序识别事件。
QVariant FrameTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role!=Qt::DisplayRole) return {};
    if(orientation==Qt::Vertical) return section+1;
    const QStringList labels{QStringLiteral("记录 ID"),QStringLiteral("Present ms"),QStringLiteral("帧间隔 ms"),QStringLiteral("长帧")};
    return section>=0 && section<labels.size() ? labels[section] : QVariant();
}
/// 把Qt表头排序转为后台请求，避免GUI线程承担大数组排序。
void FrameTableModel::sort(int column, Qt::SortOrder order) {
    // 不在GUI线程对百万QVariant做排序；控制器以最新请求模式后台重建行索引。
    if(column>=0 && column<4) emit sortRequested(column,order==Qt::DescendingOrder);
}
}
