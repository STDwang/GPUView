/// @file event_analysis_controller.cpp
/// @brief 后台搜索生命周期：取消、最新请求合并、GUI发布与退出等待。
#include "application/event_analysis_controller.h"
namespace gpuview {
/// 析构前断开完成回调并等待工作线程，防止访问已销毁UI。
EventAnalysisController::~EventAnalysisController() {
    cancel();
    if(worker_) { disconnect(worker_,nullptr,this,nullptr); worker_->wait(); delete worker_; }
}
/// 代次失效和原子取消同时执行，即使Worker刚完成也不能发布旧结果。
void EventAnalysisController::cancel(bool preserveCache) {
    ++generation_; pending_.take(); result_.reset();
    if(!preserveCache) cache_.reset();
    if(cancel_) cancel_->store(true,std::memory_order_relaxed);
}
/// 最新请求最多占一个待办；不复用旧Worker中的可变筛选参数。
void EventAnalysisController::request(Snapshot source,EventFilter filter) {
    Request next{std::move(source),std::move(filter),++generation_};
    if(worker_) { pending_.replace(std::move(next)); cancel_->store(true,std::memory_order_relaxed); }
    else start(std::move(next));
}
/// Worker按值捕获源数据，完成后GUI以generation决定是否发布。
void EventAnalysisController::start(Request request) {
    if(!canReuseEventSelection(cache_,request.source,request.filter)) cache_.reset();
    const auto previous=cache_;
    cancel_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=cancel_; auto result=std::make_shared<EventAnalysisPtr>(); auto error=std::make_shared<QString>();
    // 生产者只计算结果容器，不读取任何控件状态。
    worker_=QThread::create([request,cancel,result,error,previous] {
        try { *result=analyzeEvents(request.source,request.filter,cancel,previous); }
        catch(const Cancelled&) {}
        catch(const std::exception& e) { *error=QString::fromUtf8(e.what()); }
    });
    worker_->setParent(this); auto* thread=worker_;
    // 队列通知回GUI；wait建立结果读取同步，随后处理唯一最新待办。
    connect(thread,&QThread::finished,this,[this,thread,result,error,generation=request.generation] {
        thread->wait(); worker_=nullptr; thread->deleteLater();
        if(generation==generation_) {
            if(*result) { result_=*result; cache_=*result; emit ready(); }
            else if(!error->isEmpty()) emit failed(*error);
        }
        if(auto next=pending_.take()) start(std::move(*next));
    },Qt::QueuedConnection);
    worker_->start();
}
}
