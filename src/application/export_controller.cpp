#include "application/export_controller.h"
#include <QFileInfo>
namespace gpuview {
ExportController::~ExportController() {
    poll_.stop(); cancel();
    if(worker_) { disconnect(worker_,nullptr,this,nullptr); worker_->wait(); delete worker_; }
}
void ExportController::cancel() { if(cancel_) cancel_->store(true,std::memory_order_relaxed); }
bool ExportController::request(const QString& path,FrameAnalysisPtr analysis,ExportFormat format,
    const QString& notes,const QString& protectedInput) {
    if(busy() || !analysis) return false;
    if(!protectedInput.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath()==QFileInfo(protectedInput).canonicalFilePath()) {
        emit message(QStringLiteral("不能覆盖当前导入文件，请选择另一个导出位置。")); return false;
    }
    cancel_=std::make_shared<std::atomic_bool>(false); const auto cancel=cancel_;
    progress_=std::make_shared<std::atomic_int>(0); const auto progress=progress_;
    disconnect(&poll_,nullptr,this,nullptr); poll_.setInterval(100);
    connect(&poll_,&QTimer::timeout,this,[this] { emit progressChanged(progress_->load(std::memory_order_relaxed)); });
    poll_.start();
    struct Result { QString error; bool cancelled=false; }; auto result=std::make_shared<Result>();
    // 冻结点击导出时的不可变分析快照；后续切换选区不改变正在写的报告。
    worker_=QThread::create([path,analysis,format,notes,cancel,result,progress] {
        try { saveAnalysis(path,*analysis,format,notes,cancel,[progress](int p) { progress->store(p,std::memory_order_relaxed); }); }
        catch(const Cancelled&) { result->cancelled=true; }
        catch(const std::exception& e) { result->error=QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this); auto* thread=worker_;
    connect(thread,&QThread::finished,this,[this,thread,path,result] {
        thread->wait(); poll_.stop(); worker_=nullptr; thread->deleteLater(); emit busyChanged(false);
        if(result->cancelled) emit message(QStringLiteral("导出已取消，原文件保持不变。"));
        else if(!result->error.isEmpty()) emit message(QStringLiteral("导出失败：%1").arg(result->error));
        else emit completed(path);
    },Qt::QueuedConnection);
    emit busyChanged(true); thread->start(); return true;
}
}
