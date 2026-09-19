/// @file src/ui_widgets/frame_details_panel.h
/// @brief 完整帧明细与导出操作面板；Qt父子关系管理控件，稳定ID连接表格和时间轴。
#pragma once
#include "ui_widgets/frame_table_model.h"
#include "adapters/analysis_export.h"
#include <QWidget>
class QTableView; class QLabel; class QLineEdit; class QPushButton;
namespace gpuview {
/// GUI线程中的明细操作面板，持有模型与视图并以稳定事件ID联动。
class FrameDetailsPanel : public QWidget {
    Q_OBJECT
public:
    /// 创建表格模型及导出控件并连接信号；子对象归面板所有，不为每帧创建控件。
    explicit FrameDetailsPanel(QWidget* parent = nullptr);
    /// GUI线程绑定只读分析并刷新显示；空值表示当前分析未就绪，不继续展示旧范围数据。
    void setAnalysis(FrameAnalysisPtr analysis);
    /// 按稳定ID恢复选择并阻断信号，避免表格与时间轴联动递归；不存在时不误选其他行。
    void selectId(std::uint64_t id);
    /// 换会话时清空ID、备注和分析，防止不同会话ID碰撞串选。
    void resetSession();
    /// 更新导出按钮状态；后台写入期间可继续阅读，但禁止重复发起导出。
    void setExportBusy(bool busy);
    /// 把采样百分比显示在取消按钮上，不按每条帧记录刷新UI。
    void setExportProgress(int percent);
    /// 返回只读分析快照副本；导出方可持有它冻结当前范围与排序。
    FrameAnalysisPtr analysis() const { return model_->analysis(); }
signals:
    /// 发布单击选中的源事件供高亮；当前联动在GUI线程同步消费引用。
    void eventSelected(const gpuview::Event& event);
    /// 发布双击/Enter激活的事件供缩放定位，不以行号表示事件身份。
    void eventActivated(const gpuview::Event& event);
    /// 请求按column升/降序后台排序，不在GUI线程同步排序百万QVariant。
    void sortRequested(int column, bool descending);
    /// 发布格式与备注；主窗口补充目标路径和冻结分析后交给控制器。
    void exportRequested(gpuview::ExportFormat format, const QString& notes);
    /// 请求控制器置位取消标志，让导出Worker协作退出。
    void cancelExport();
private:
    /// 面板拥有的表格模型，按需提供单元格数据。
    FrameTableModel* model_;
    /// 面板拥有的表格视图，用稳定ID关联单选行与源事件。
    QTableView* table_;
    /// 面板拥有的数量及操作提示标签。
    QLabel* label_;
    /// 面板拥有的报告备注输入框，发起导出时复制内容。
    QLineEdit* notes_;
    /// 面板拥有的格式/导出按钮，写入期间禁止重复提交。
    QPushButton* exportButton_;
    /// 面板拥有的取消按钮，同时显示采样进度。
    QPushButton* cancelButton_;
    /// 跨排序保留的稳定ID；换会话清空，不保存失效行号。
    std::optional<std::uint64_t> selectedId_;
    /// 导出忙状态，与分析就绪状态共同决定按钮可用性。
    bool exporting_ = false;
};
}
