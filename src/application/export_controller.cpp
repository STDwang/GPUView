/// @file src/application/export_controller.cpp
/// @brief GUI线程的导出任务协调器；冻结快照，后台写入，定时进度和协作取消。
#include "application/export_controller.h"
#include <QFileInfo>
namespace gpuview {
/// 停止轮询、协作取消并等待Worker，避免线程仍访问已销毁状态。
ExportController::~ExportController() {
    poll_.stop(); cancel();
    if(worker_) { disconnect(worker_,nullptr,this,nullptr); worker_->wait(); delete worker_; }
}
/// 置位共享取消标志，Worker在检查点停止；不能强杀线程，也不能撤销已提交文件。
void ExportController::cancel() { if(cancel_) cancel_->store(true,std::memory_order_relaxed); }
/// 启动单个冻结快照导出；忙碌、空分析或目标为当前输入时拒绝，返回是否已受理。
bool ExportController::request(const QString& path,FrameAnalysisPtr analysis,ExportFormat format,
    const QString& notes,const QString& protectedInput) {
    if(busy() || !analysis) return false;
    if(!protectedInput.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath()==QFileInfo(protectedInput).canonicalFilePath()) {
        emit message(QStringLiteral("不能覆盖当前导入文件，请选择另一个导出位置。")); return false;
    }
    cancel_=std::make_shared<std::atomic_bool>(false); const auto cancel=cancel_;
    progress_=std::make_shared<std::atomic_int>(0); const auto progress=progress_;
    disconnect(&poll_,nullptr,this,nullptr); poll_.setInterval(100);
    // GUI定时读取最新原子值，进度生产速度不影响排队消息数量。
    connect(&poll_,&QTimer::timeout,this,[this] { emit progressChanged(progress_->load(std::memory_order_relaxed)); });
    poll_.start();
    /// Worker写入、线程完成后GUI读取的任务结果；通过线程完成/等待建立读取时序。
    struct Result {
        /// 后台捕获的错误文本，GUI在任务完成后读取展示。
        QString error;
        /// 任务因协作取消退出，与格式/读取错误区分。
        bool cancelled=false;
    }; auto result=std::make_shared<Result>();
    // 冻结点击导出时的不可变分析快照；后续切换选区不改变正在写的报告。
    // Worker按值持有分析和输出参数，只写文件及结果对象，不访问窗口。
    worker_=QThread::create([path,analysis,format,notes,cancel,result,progress] {
        // 进度回调在Worker执行，只覆盖原子邮箱，不逐帧向GUI发信号。
        try { saveAnalysis(path,*analysis,format,notes,cancel,[progress](int p) { progress->store(p,std::memory_order_relaxed); }); }
        catch(const Cancelled&) { result->cancelled=true; }
        catch(const std::exception& e) { result->error=QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this); auto* thread=worker_;
    // 队列回调在GUI执行；先等待线程结束再读结果，随后释放线程并发布完成/失败状态。
    connect(thread,&QThread::finished,this,[this,thread,path,result] {
        thread->wait(); poll_.stop(); worker_=nullptr; thread->deleteLater(); emit busyChanged(false);
        if(result->cancelled) emit message(QStringLiteral("导出已取消，原文件保持不变。"));
        else if(!result->error.isEmpty()) emit message(QStringLiteral("导出失败：%1").arg(result->error));
        else emit completed(path);
    },Qt::QueuedConnection);
    emit busyChanged(true); thread->start(); return true;
}
}
