/// @file src/ui_widgets/frame_table_model.h
/// @brief 惰性只读Model/View适配器；不创建逐帧控件，排序交给后台，源数据由快照拥有。
#pragma once
#include "core/frame_analysis.h"
#include <QAbstractTableModel>
namespace gpuview {
/// 平面只读四列表格模型，按需格式化单元格并转发后台排序请求。
class FrameTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    /// 创建平面只读模型；parent管理QObject寿命，数据由共享分析快照拥有。
    explicit FrameTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    /// GUI线程绑定只读分析并刷新显示；空值表示当前分析未就绪，不继续展示旧范围数据。
    void setAnalysis(FrameAnalysisPtr analysis);
    /// 返回只读分析快照副本；导出方可持有它冻结当前范围与排序。
    FrameAnalysisPtr analysis() const { return analysis_; }
    /// 借用行对应源事件，越界返回nullptr；不能在其快照释放后继续使用。
    const Event* eventAt(int row) const;
    /// 按稳定事件ID查当前排序行，不存在返回-1，避免跨排序沿用行号。
    int rowForId(std::uint64_t id) const { return analysis_ ? analysis_->rowForId(id) : -1; }
    /// 返回完整帧行数；平面模型不提供子节点，非空parent返回0。
    int rowCount(const QModelIndex& parent = {}) const override;
    /// 固定提供ID、Present时刻、间隔和长帧四列；子节点无列。
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 4; }
    /// 按Qt角色惰性生成单元格，DisplayRole格式化毫秒，UserRole保留稳定ID。
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    /// 提供列名和行序号；行序号只是位置，不能跨排序识别事件。
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    /// 把Qt表头排序转为后台请求，避免GUI线程承担大数组排序。
    void sort(int column, Qt::SortOrder order) override;
signals:
    /// 请求按column升/降序后台排序，不在GUI线程同步排序百万QVariant。
    void sortRequested(int column, bool descending);
private:
    /// 共享只读分析，延长源数据寿命；GUI发布时整体替换。
    FrameAnalysisPtr analysis_;
};
}
