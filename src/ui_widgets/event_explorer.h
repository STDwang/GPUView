/// @file event_explorer.h
/// @brief 事件与名称汇总的只读Model/View面板，搜索由后台控制器执行。
#pragma once
#include "application/event_analysis_controller.h"
#include <QAbstractTableModel>
#include <QWidget>
#include <QTimer>
class QLineEdit; class QDoubleSpinBox; class QCheckBox; class QLabel; class QTableView; class QPushButton;
namespace gpuview {
/// 同一分析快照的事件/汇总适配器，不创建逐记录QObject。
class EventAnalysisModel : public QAbstractTableModel {
    Q_OBJECT
public:
    /// groups决定显示逐事件或按名称汇总，parent负责QObject寿命。
    EventAnalysisModel(bool groups,QObject* parent=nullptr):QAbstractTableModel(parent),groups_(groups) {}
    /// GUI线程整体替换结果，空指针清除过期行。
    void setResult(EventAnalysisPtr result);
    /// 返回当前只读分析，供选择恢复和测试读取。
    EventAnalysisPtr result() const { return result_; }
    /// 平面模型只在根索引返回行数。
    int rowCount(const QModelIndex& parent={}) const override;
    /// 事件6列、汇总5列，子节点无列。
    int columnCount(const QModelIndex& parent={}) const override { return parent.isValid()?0:groups_?5:6; }
    /// 按角色惰性提供单元格，数值转换为毫秒。
    QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
    /// 显示字段及范围贡献口径，不用行号充当事件ID。
    QVariant headerData(int section,Qt::Orientation orientation,int role) const override;
    /// 只转发排序请求，不在GUI执行全量排序。
    void sort(int column,Qt::SortOrder order) override;
    /// 事件行返回原事件，汇总行返回最大贡献事件；越界返回空。
    const Event* eventAt(int row) const;
signals:
    /// 分别请求事件或汇总按指定列后台排序。
    void sortRequested(bool groups,int column,bool descending);
private:
    /// 是否展示名称汇总，构造后不变。
    bool groups_;
    /// 持有结果及源快照，保护事件指针寿命。
    EventAnalysisPtr result_;
};
/// 搜索条件、双表格与导航的组合面板；所有重计算在Worker。
class EventExplorer : public QWidget {
    Q_OBJECT
public:
    /// 组装控件和连接，180ms防抖只合并输入，不在GUI扫描事件。
    explicit EventExplorer(QWidget* parent=nullptr);
    /// 更新会话/可用轨道/当前选区；相同上下文不重复计算。
    void setContext(Snapshot source,std::vector<std::uint32_t> tracks,std::optional<TimeRange> selection);
    /// 以稳定ID选择事件，信号阻断避免双向联动递归。
    void selectId(std::uint64_t id);
    /// 将键盘焦点移到名称输入并选中文字，供窗口搜索快捷键调用。
    void focusSearch();
signals:
    /// 单击事件高亮时间轴，不改变分析范围。
    void eventSelected(const gpuview::Event& event);
    /// 双击或导航定位时间轴，汇总行定位最大贡献事件。
    void eventActivated(const gpuview::Event& event);
private:
    /// 立即取消旧代次并清空旧结果，延迟发送新查询。
    void schedule();
    /// 读取GUI条件并冻结为纯数据参数发送Worker。
    void submit();
    /// 表头、回车等明确操作跳过输入防抖，仍经同一取消/代次链提交。
    void submitNow();
    /// 批量恢复筛选默认值，只发起一次新查询，保留用户排序。
    void resetFilters();
    /// 在当前排序中循环导航；空结果不执行。
    void navigate(int step);
    /// 控制导航和取消按钮状态，避免空表仍可点击。
    void setReady(bool ready);
    /// 本面板拥有的后台协调器，销毁时取消并等待。
    EventAnalysisController controller_;
    /// 防抖定时器，由GUI线程触发。
    QTimer debounce_;
    /// 当前源快照，换会话时清空选中ID。
    Snapshot source_;
    /// 当前显示/选择的源轨道ID集合。
    std::vector<std::uint32_t> tracks_;
    /// 上层框选范围，未选择时为空。
    std::optional<TimeRange> selection_;
    /// 排序条件和提交参数，submit时覆盖筛选值。
    EventFilter filter_;
    /// 跨排序保留的事件身份。
    std::optional<std::uint64_t> selectedId_;
    /// 面板拥有的名称字面子串输入框。
    QLineEdit* text_;
    /// 面板拥有的完整时长下限，单位毫秒。
    QDoubleSpinBox* minimum_;
    /// 面板拥有的完整时长上限，0表示不限。
    QDoubleSpinBox* maximum_;
    /// 是否限制当前选区，无选区时回到全会话。
    QCheckBox* selectedOnly_;
    /// 查询状态、数据来源和口径说明。
    QLabel* status_;
    /// 逐事件模型，面板拥有。
    EventAnalysisModel* events_;
    /// 名称汇总模型，面板拥有。
    EventAnalysisModel* groups_;
    /// 逐事件表格，面板拥有。
    QTableView* table_;
    /// 汇总表格，面板拥有。
    QTableView* summary_;
    /// 上一条导航按钮，面板拥有。
    QPushButton* previous_;
    /// 下一条导航按钮，面板拥有。
    QPushButton* next_;
    /// 取消按钮，面板拥有。
    QPushButton* cancel_;
};
}
