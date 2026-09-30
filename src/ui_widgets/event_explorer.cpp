/// @file event_explorer.cpp
/// @brief Nsight Systems Events View启发的搜索/汇总联动；不声称支持其私有报告或采集能力。
#include "ui_widgets/event_explorer.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QTableView>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSignalBlocker>
namespace gpuview {
/// 用模型重置发布整份不可变结果，避免逐行插入百万条通知。
void EventAnalysisModel::setResult(EventAnalysisPtr result) {
    beginResetModel(); result_=std::move(result); endResetModel();
}
/// 平面模型不提供子行，Qt只需读取可见单元格。
int EventAnalysisModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || !result_ ? 0 : int(groups_?result_->groups.size():result_->rows.size());
}
/// 将显示行映射回源事件，汇总定位的是贡献最大项而不是第一项。
const Event* EventAnalysisModel::eventAt(int row) const {
    if(row<0 || row>=rowCount()) return nullptr;
    return groups_?result_->groups[std::size_t(row)].representative:result_->rows[std::size_t(row)].event;
}
/// 显示角色仅转换当前请求单元格；工具提示提供完整长名称。
QVariant EventAnalysisModel::data(const QModelIndex& index,int role) const {
    if(!index.isValid() || index.column()<0 || index.column()>=columnCount()) return {};
    const auto* event=eventAt(index.row()); if(!event) return {};
    if(role==Qt::UserRole) return QVariant::fromValue(qulonglong(event->id));
    if(role==Qt::TextAlignmentRole) {
        const bool number=groups_?index.column()>0:(index.column()==0 || index.column()>=3);
        return int((number?Qt::AlignRight:Qt::AlignLeft)|Qt::AlignVCenter);
    }
    if(role!=Qt::DisplayRole && role!=Qt::ToolTipRole) return {};
    if(groups_) {
        const auto& group=result_->groups[std::size_t(index.row())];
        switch(index.column()) {
        case 0:return QString::fromStdString(group.name);
        case 1:return QVariant::fromValue(qulonglong(group.count));
        case 2:return QString::number(double(group.totalMs),'f',6);
        case 3:return QString::number(double(group.totalMs/group.count),'f',6);
        default:return QString::number(double(group.maximum)/1e6,'f',6);
        }
    }
    switch(index.column()) {
    case 0:return QVariant::fromValue(qulonglong(event->id));
    case 1:return QString::fromStdString(result_->source->names[event->name]);
    case 2:return QString::fromStdString(result_->source->tracks[event->track].name);
    case 3:return QString::number(double(event->start)/1e6,'f',6);
    case 4:return QString::number(double(event->duration)/1e6,'f',6);
    default:return QString::number(double(result_->rows[std::size_t(index.row())].contribution)/1e6,'f',6);
    }
}
/// 表头明确完整时长与选区贡献不同，所有显示时间单位均为毫秒。
QVariant EventAnalysisModel::headerData(int section,Qt::Orientation orientation,int role) const {
    if(role!=Qt::DisplayRole) return {};
    if(orientation==Qt::Vertical) return section+1;
    const QStringList labels=groups_?QStringList{QStringLiteral("名称"),QStringLiteral("次数"),
        QStringLiteral("贡献总和 ms"),QStringLiteral("平均贡献 ms"),QStringLiteral("最大贡献 ms")}:
        QStringList{QStringLiteral("ID"),QStringLiteral("名称"),QStringLiteral("轨道"),
        QStringLiteral("开始 ms"),QStringLiteral("完整时长 ms"),QStringLiteral("范围贡献 ms")};
    return section>=0 && section<labels.size()?QVariant(labels[section]):QVariant();
}
/// 用户排序由控制器重算，UI不对海量QVariant使用代理排序。
void EventAnalysisModel::sort(int column,Qt::SortOrder order) {
    if(column>=0 && column<columnCount()) emit sortRequested(groups_,column,order==Qt::DescendingOrder);
}
/// 只创建固定数量控件，表格行数由模型描述。
EventExplorer::EventExplorer(QWidget* parent):QWidget(parent) {
    setObjectName("eventExplorer");
    setToolTip(QStringLiteral("筛选只作用于本面板；现有帧分析导出仍使用统计页的组和选区，不包含这里的名称/时长筛选。"));
    auto* layout=new QVBoxLayout(this); auto* searchRow=new QHBoxLayout; auto* controls=new QHBoxLayout;
    text_=new QLineEdit; text_->setObjectName("eventSearch");
    text_->setPlaceholderText(QStringLiteral("搜索事件名称 · 区分大小写 · Ctrl+F")); text_->setClearButtonEnabled(true);
    text_->setAccessibleName(QStringLiteral("搜索事件名称")); searchRow->addWidget(text_,1);
    auto* reset=new QPushButton(QStringLiteral("重置筛选")); reset->setObjectName("eventReset");
    reset->setToolTip(QStringLiteral("清空名称和时长条件，恢复仅选区；保留当前排序。")); searchRow->addWidget(reset);
    minimum_=new QDoubleSpinBox; maximum_=new QDoubleSpinBox;
    minimum_->setObjectName("eventMinimum"); maximum_->setObjectName("eventMaximum");
    for(auto* spin:{minimum_,maximum_}) { spin->setRange(0,1e9); spin->setDecimals(6); spin->setSuffix(" ms"); }
    maximum_->setSpecialValueText(QStringLiteral("不限"));
    controls->addWidget(new QLabel(QStringLiteral("完整时长 ≥"))); controls->addWidget(minimum_);
    controls->addWidget(new QLabel(QStringLiteral("≤"))); controls->addWidget(maximum_);
    selectedOnly_=new QCheckBox(QStringLiteral("仅选区")); selectedOnly_->setObjectName("eventSelectionOnly");
    selectedOnly_->setChecked(true); controls->addWidget(selectedOnly_);
    previous_=new QPushButton(QStringLiteral("上一条")); next_=new QPushButton(QStringLiteral("下一条"));
    previous_->setObjectName("eventPrevious"); next_->setObjectName("eventNext");
    cancel_=new QPushButton(QStringLiteral("取消")); cancel_->setObjectName("eventCancel");
    searchRow->addWidget(previous_); searchRow->addWidget(next_); searchRow->addWidget(cancel_);
    controls->addStretch(); layout->addLayout(searchRow); layout->addLayout(controls);
    status_=new QLabel(QStringLiteral("加载数据后可搜索事件")); status_->setWordWrap(true); layout->addWidget(status_);
    events_=new EventAnalysisModel(false,this); groups_=new EventAnalysisModel(true,this);
    events_->setObjectName("eventResultsModel"); groups_->setObjectName("eventGroupsModel");
    table_=new QTableView; summary_=new QTableView;
    table_->setObjectName("eventResults"); summary_->setObjectName("eventGroups");
    table_->setModel(events_); summary_->setModel(groups_);
    for(auto* table:{table_,summary_}) {
        table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setAlternatingRowColors(true);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setDefaultSectionSize(30); table->setShowGrid(false);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSortingEnabled(true);
    }
    table_->sortByColumn(3,Qt::AscendingOrder); summary_->sortByColumn(2,Qt::DescendingOrder);
    auto* tabs=new QTabWidget; tabs->addTab(table_,QStringLiteral("事件明细")); tabs->addTab(summary_,QStringLiteral("名称汇总"));
    tabs->setTabToolTip(0,QStringLiteral("单击高亮，双击定位；上下条按当前事件排序循环。"));
    tabs->setTabToolTip(1,QStringLiteral("同名事件聚合；双击定位贡献最大项。并发时长求和不是利用率。"));
    layout->addWidget(tabs,1);
    debounce_.setSingleShot(true); debounce_.setInterval(180);
    connect(&debounce_,&QTimer::timeout,this,&EventExplorer::submit);
    connect(text_,&QLineEdit::textChanged,this,&EventExplorer::schedule);
    connect(reset,&QPushButton::clicked,this,&EventExplorer::resetFilters);
    // 回车立即提交，停止防抖计时，避免同一输入重复提交。
    connect(text_,&QLineEdit::returnPressed,this,[this] { schedule(); debounce_.stop(); submit(); });
    connect(minimum_,&QDoubleSpinBox::valueChanged,this,&EventExplorer::schedule);
    connect(maximum_,&QDoubleSpinBox::valueChanged,this,&EventExplorer::schedule);
    connect(selectedOnly_,&QCheckBox::toggled,this,&EventExplorer::schedule);
    // 两表分别保存排序键，重算后仍按事件ID恢复选择。
    for(auto* model:{events_,groups_}) connect(model,&EventAnalysisModel::sortRequested,this,[this](bool groups,int column,bool descending) {
        if(groups) { filter_.groupColumn=column; filter_.groupDescending=descending; }
        else { filter_.eventColumn=column; filter_.eventDescending=descending; }
        schedule();
    });
    // 普通行选择只高亮，不改变搜索范围；时间轴反向恢复时使用信号阻断。
    connect(table_->selectionModel(),&QItemSelectionModel::currentRowChanged,this,[this](const QModelIndex& index) {
        if(const auto* event=events_->eventAt(index.row())) { selectedId_=event->id; emit eventSelected(*event); }
    });
    // 双击激活事件或汇总代表事件，范围条件保持为原先框选。
    connect(table_,&QTableView::activated,this,[this](const QModelIndex& i) {
        if(const auto* event=events_->eventAt(i.row())) emit eventActivated(*event);
    });
    connect(summary_,&QTableView::activated,this,[this](const QModelIndex& i) {
        if(const auto* event=groups_->eventAt(i.row())) emit eventActivated(*event);
    });
    connect(previous_,&QPushButton::clicked,this,[this]{navigate(-1);});
    connect(next_,&QPushButton::clicked,this,[this]{navigate(1);});
    // 手动取消会停止尚未触发的防抖任务，并使运行任务的代次失效。
    connect(cancel_,&QPushButton::clicked,this,[this] {
        debounce_.stop(); controller_.cancel(); setReady(false); cancel_->setEnabled(false);
        status_->setText(QStringLiteral("搜索已取消；修改条件重新查询。"));
    });
    // 只绑定同一份已发布结果，模型重置期间不反馈行选择事件。
    connect(&controller_,&EventAnalysisController::ready,this,[this] {
        const QSignalBlocker block(table_->selectionModel());
        const auto result=controller_.result(); events_->setResult(result); groups_->setResult(result);
        if(selectedId_) selectId(*selectedId_);
        setReady(true); cancel_->setEnabled(false);
        status_->setText(QStringLiteral("%1 · %2 条事件 / %3 个名称 · [%4, %5) ms · %6")
            .arg(result->source->synthetic?QStringLiteral("教学模拟"):QStringLiteral("外部输入"))
            .arg(qulonglong(result->rows.size())).arg(qulonglong(result->groups.size()))
            .arg(double(result->filter.range.begin)/1e6,0,'f',3).arg(double(result->filter.range.end)/1e6,0,'f',3)
            .arg(result->rows.empty()?QStringLiteral("无匹配，请调整筛选"):QStringLiteral("单击高亮 · 双击定位")));
        status_->setToolTip(result->source->frames?QStringLiteral("Present归属，贡献为完整间隔，非GPU时长。"):
            QStringLiteral("Trace贡献按选区裁剪，并发求和非利用率。"));
    });
    connect(&controller_,&EventAnalysisController::failed,this,[this](const QString& error) {
        setReady(false); cancel_->setEnabled(false); status_->setText(QStringLiteral("搜索失败：")+error);
    });
    setReady(false); cancel_->setEnabled(false);
}
/// 聚焦而不改动条件或发起查询，保留当前结果。
void EventExplorer::focusSearch() { text_->setFocus(Qt::ShortcutFocusReason); text_->selectAll(); }
/// 阻断四个输入的逐项通知，批量重置后仅调度一次。
void EventExplorer::resetFilters() {
    const QSignalBlocker textBlock(text_),minimumBlock(minimum_),maximumBlock(maximum_),selectionBlock(selectedOnly_);
    text_->clear(); minimum_->setValue(0); maximum_->setValue(0); selectedOnly_->setChecked(true);
    schedule(); focusSearch();
}
/// 相同上下文不因帧表独立排序而重复搜索，换会话不继承旧ID。
void EventExplorer::setContext(Snapshot source,std::vector<std::uint32_t> tracks,std::optional<TimeRange> selection) {
    const bool sameSelection=(!selection_ && !selection) || (selection_ && selection && *selection_==*selection);
    if(source_==source && tracks_==tracks && sameSelection) return;
    if(source_!=source) { selectedId_.reset(); controller_.cancel(); }
    source_=std::move(source); tracks_=std::move(tracks); selection_=selection; schedule();
}
/// 新输入立即失效旧结果，不能在防抖等待期间点击过期行。
void EventExplorer::schedule() {
    controller_.cancel(true); debounce_.stop();
    const QSignalBlocker block(table_->selectionModel()); events_->setResult({}); groups_->setResult({});
    setReady(false); cancel_->setEnabled(bool(source_));
    status_->setText(source_?QStringLiteral("后台搜索中… 修改条件只保留最新请求。"):QStringLiteral("尚无数据"));
    if(source_) debounce_.start();
}
/// 以毫秒输入构造纳秒过滤；上限0专门表示无限，不与空结果混淆。
void EventExplorer::submit() {
    if(!source_) return;
    filter_.text=text_->text().toStdString(); filter_.tracks=tracks_;
    filter_.range=selectedOnly_->isChecked() && selection_?*selection_:source_->bounds;
    filter_.minimum=TimeNs(minimum_->value()*1e6);
    filter_.maximum=maximum_->value()>0?std::optional<TimeNs>(TimeNs(maximum_->value()*1e6)):std::nullopt;
    controller_.request(source_,filter_);
}
/// 二分查找事件身份，未匹配时清空可见选择但保留待恢复ID。
void EventExplorer::selectId(std::uint64_t id) {
    selectedId_=id; const auto result=events_->result(); if(!result) return;
    const QSignalBlocker block(table_->selectionModel()); const int row=result->rowForId(id);
    table_->setCurrentIndex(row>=0?events_->index(row,0):QModelIndex());
    if(row>=0) table_->scrollTo(events_->index(row,0));
}
/// 空表禁用导航；首次下一条从首行、上一条从末行开始。
void EventExplorer::navigate(int step) {
    const int count=events_->rowCount(); if(!count) return;
    const int current=table_->currentIndex().row();
    const int row=current<0?(step>0?0:count-1):int((qint64(current)+step+count)%count);
    table_->setCurrentIndex(events_->index(row,0)); table_->scrollTo(events_->index(row,0));
    if(const auto* event=events_->eventAt(row)) emit eventActivated(*event);
}
/// 是否可导航取决于有效结果与非空事件列表，而非是否加载过数据。
void EventExplorer::setReady(bool ready) {
    previous_->setEnabled(ready && events_->rowCount()>0); next_->setEnabled(ready && events_->rowCount()>0);
}
}
