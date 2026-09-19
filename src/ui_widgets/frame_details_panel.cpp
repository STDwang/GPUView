/// @file src/ui_widgets/frame_details_panel.cpp
/// @brief 完整帧明细与导出操作面板；Qt父子关系管理控件，稳定ID连接表格和时间轴。
#include "ui_widgets/frame_details_panel.h"
#include <QTableView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QVBoxLayout>
#include <QSignalBlocker>
namespace gpuview {
/// 创建表格模型及导出控件并连接信号；子对象归面板所有，不为每帧创建控件。
FrameDetailsPanel::FrameDetailsPanel(QWidget* parent) : QWidget(parent) {
    auto* layout=new QVBoxLayout(this); auto* actions=new QHBoxLayout;
    label_=new QLabel(QStringLiteral("暂无帧明细")); actions->addWidget(label_,1);
    notes_=new QLineEdit; notes_->setPlaceholderText(QStringLiteral("导出备注（仅保存到本地报告）")); notes_->setMaxLength(2000); actions->addWidget(notes_,1);
    exportButton_=new QPushButton(QStringLiteral("导出分析")); exportButton_->setObjectName("exportAnalysis"); exportButton_->setEnabled(false);
    auto* menu=new QMenu(exportButton_);
    for(const auto& option: {std::make_pair(QStringLiteral("CSV 摘要"),ExportFormat::SummaryCsv),
        std::make_pair(QStringLiteral("Markdown 报告"),ExportFormat::Markdown),std::make_pair(QStringLiteral("全部所选帧 CSV"),ExportFormat::FramesCsv)}) {
        // 菜单回调按值捕获该项格式，点击时读取备注，避免循环变量捕获串项。
        menu->addAction(option.first,this,[this,format=option.second] { emit exportRequested(format,notes_->text()); });
    }
    exportButton_->setMenu(menu); actions->addWidget(exportButton_);
    cancelButton_=new QPushButton(QStringLiteral("取消导出")); cancelButton_->setEnabled(false); actions->addWidget(cancelButton_);
    connect(cancelButton_,&QPushButton::clicked,this,&FrameDetailsPanel::cancelExport); layout->addLayout(actions);
    model_=new FrameTableModel(this); table_=new QTableView; table_->setObjectName("frameDetailsTable"); table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed); table_->verticalHeader()->setDefaultSectionSize(24);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicator(1,Qt::AscendingOrder); table_->setMinimumHeight(130); layout->addWidget(table_,1);
    connect(model_,&FrameTableModel::sortRequested,this,&FrameDetailsPanel::sortRequested);
    // 单行选择转换为稳定事件ID和源事件；后续排序恢复用ID而不是当前行号。
    connect(table_->selectionModel(),&QItemSelectionModel::currentRowChanged,this,[this](const QModelIndex& current,const QModelIndex&) {
        if(const auto* e=model_->eventAt(current.row())) { selectedId_=e->id; emit eventSelected(*e); }
    });
    // 双击/Enter把有效行转换为源事件交给时间轴定位。
    connect(table_,&QTableView::activated,this,[this](const QModelIndex& index) { if(const auto* e=model_->eventAt(index.row())) emit eventActivated(*e); });
}
/// GUI线程绑定只读分析并刷新显示；空值表示当前分析未就绪，不继续展示旧范围数据。
void FrameDetailsPanel::setAnalysis(FrameAnalysisPtr analysis) {
    const QSignalBlocker blocker(table_->selectionModel()); model_->setAnalysis(std::move(analysis));
    const auto analysisSnapshot=model_->analysis();
    label_->setText(analysisSnapshot ? QStringLiteral("完整帧明细：%1 条 · 单击联动，双击/Enter定位 · 点击表头后台排序").arg(analysisSnapshot->rows.size()) : QStringLiteral("等待当前选区分析…"));
    if(selectedId_) selectId(*selectedId_);
    exportButton_->setEnabled(bool(analysisSnapshot) && !exporting_);
}
/// 按稳定ID恢复选择并阻断信号，避免表格与时间轴联动递归；不存在时不误选其他行。
void FrameDetailsPanel::selectId(std::uint64_t id) {
    selectedId_=id; const int row=model_->rowForId(id);
    const QSignalBlocker blocker(table_->selectionModel());
    if(row<0) { table_->clearSelection(); table_->setCurrentIndex({}); return; }
    table_->selectRow(row); table_->setCurrentIndex(model_->index(row,0)); table_->scrollTo(model_->index(row,0));
}
/// 换会话时清空ID、备注和分析，防止不同会话ID碰撞串选。
void FrameDetailsPanel::resetSession() { selectedId_.reset(); notes_->clear(); setAnalysis({}); }
/// 把采样百分比显示在取消按钮上，不按每条帧记录刷新UI。
void FrameDetailsPanel::setExportProgress(int percent) { cancelButton_->setText(QStringLiteral("取消导出 (%1%)").arg(percent)); }
/// 更新导出按钮状态；后台写入期间可继续阅读，但禁止重复发起导出。
void FrameDetailsPanel::setExportBusy(bool busy) {
    cancelButton_->setText(QStringLiteral("取消导出"));
    exporting_=busy; cancelButton_->setEnabled(busy); exportButton_->setEnabled(!busy && bool(model_->analysis()));
}
}
