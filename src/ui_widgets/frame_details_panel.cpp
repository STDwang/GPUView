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
FrameDetailsPanel::FrameDetailsPanel(QWidget* parent) : QWidget(parent) {
    auto* layout=new QVBoxLayout(this); auto* actions=new QHBoxLayout;
    label_=new QLabel(QStringLiteral("暂无帧明细")); actions->addWidget(label_,1);
    notes_=new QLineEdit; notes_->setPlaceholderText(QStringLiteral("导出备注（仅保存到本地报告）")); notes_->setMaxLength(2000); actions->addWidget(notes_,1);
    exportButton_=new QPushButton(QStringLiteral("导出分析")); exportButton_->setObjectName("exportAnalysis"); exportButton_->setEnabled(false);
    auto* menu=new QMenu(exportButton_);
    for(const auto& option: {std::make_pair(QStringLiteral("CSV 摘要"),ExportFormat::SummaryCsv),
        std::make_pair(QStringLiteral("Markdown 报告"),ExportFormat::Markdown),std::make_pair(QStringLiteral("全部所选帧 CSV"),ExportFormat::FramesCsv)}) {
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
    connect(table_->selectionModel(),&QItemSelectionModel::currentRowChanged,this,[this](const QModelIndex& current,const QModelIndex&) {
        if(const auto* e=model_->eventAt(current.row())) { selectedId_=e->id; emit eventSelected(*e); }
    });
    connect(table_,&QTableView::activated,this,[this](const QModelIndex& index) { if(const auto* e=model_->eventAt(index.row())) emit eventActivated(*e); });
}
void FrameDetailsPanel::setAnalysis(FrameAnalysisPtr analysis) {
    const QSignalBlocker blocker(table_->selectionModel()); model_->setAnalysis(std::move(analysis));
    const auto analysisSnapshot=model_->analysis();
    label_->setText(analysisSnapshot ? QStringLiteral("完整帧明细：%1 条 · 单击联动，双击/Enter定位 · 点击表头后台排序").arg(analysisSnapshot->rows.size()) : QStringLiteral("等待当前选区分析…"));
    if(selectedId_) selectId(*selectedId_);
    exportButton_->setEnabled(bool(analysisSnapshot) && !exporting_);
}
void FrameDetailsPanel::selectId(std::uint64_t id) {
    selectedId_=id; const int row=model_->rowForId(id);
    const QSignalBlocker blocker(table_->selectionModel());
    if(row<0) { table_->clearSelection(); table_->setCurrentIndex({}); return; }
    table_->selectRow(row); table_->setCurrentIndex(model_->index(row,0)); table_->scrollTo(model_->index(row,0));
}
void FrameDetailsPanel::resetSession() { selectedId_.reset(); notes_->clear(); setAnalysis({}); }
void FrameDetailsPanel::setExportProgress(int percent) { cancelButton_->setText(QStringLiteral("取消导出 (%1%)").arg(percent)); }
void FrameDetailsPanel::setExportBusy(bool busy) {
    cancelButton_->setText(QStringLiteral("取消导出"));
    exporting_=busy; cancelButton_->setEnabled(busy); exportButton_->setEnabled(!busy && bool(model_->analysis()));
}
}
