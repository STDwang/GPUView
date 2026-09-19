/// @file src/application/session_controller.h
/// @brief 会话加载协调器；只保留最新待办，GUI发布只读快照，失败或取消保留旧会话。
#pragma once
#include "core/trace_store.h"
#include <QObject>
#include <QThread>
#include <QTimer>

namespace gpuview {
// GUI只在固定频率读取一个原子进度值，生产速度再高也不会逐条排入事件队列。
/// 单值原子进度邮箱，生产者覆盖写入，GUI定时读取，空间不会随数据量增长。
struct ProgressMailbox {
    /// Worker写、GUI定时读的原子百分比，避免逐事件信号。
    std::atomic_int percent{0};
};
/// 加载生命周期协调器；一个运行任务加一个最新待办，所有界面发布发生在GUI线程。
class SessionController : public QObject {
    Q_OBJECT
public:
    /// 建立GUI线程的进度定时器，加载Worker通过原子邮箱交付进度。
    explicit SessionController(QObject* parent = nullptr);
    /// 停轮询、取消并等待加载线程；析构完成后不遗留访问本对象的Worker。
    ~SessionController() override;
    /// 请求生成count条教学事件；推进代次，运行期间只保留最新一个待办。
    void requestSynthetic(std::size_t count);
    /// 后台读取path；旧会话保留到新快照成功发布，失败或取消不清空分析。
    void requestFile(const QString& path);
    /// 推进代次、清空待办并置位取消，使迟到结果失效；已发布会话继续保留。
    void cancel();
    /// 返回成功导入的本地源路径，仅用于输入保护，不导出到公开报告。
    QString sourcePath() const { return sourcePath_; }
    /// 返回最近成功加载耗时（毫秒），用于状态展示与测量。
    double loadMs() const { return loadMs_; }
    /// 返回是否仍持有运行中的Worker；仅由GUI线程查询并据此更新操作状态。
    bool busy() const { return worker_ != nullptr; }
    /// 取得共享只读会话快照；副本延长数据寿命，不提供写入口。
    Snapshot snapshot() const { return current_; }
    /// 返回待办数量0或1，用于验证请求邮箱有界，不包含当前运行任务。
    std::size_t pendingRequests() const { return pending_.hasValue() ? 1 : 0; }
signals:
    /// 通知GUI已有新快照；接收方通过snapshot读取，不共享可变容器。
    void snapshotReady();
    /// 通知GUI更新忙闲状态，不用于逐记录进度通知。
    void busyChanged(bool busy);
    /// 发布0到100的定时进度采样，合并高频生产者更新以避免事件队列积压。
    void progressChanged(int percent);
    /// 向GUI发布错误、取消或提示文本；Worker不直接修改控件。
    void message(const QString& text);
private:
    /// 一次异步任务的冻结参数，代次用于完成时判断结果是否仍有效。
    struct Request {
        /// 教学模式请求生成的事件数量；文件请求不使用此值。
        std::size_t count;
        /// 发起请求时的代次，完成时用于丢弃迟到结果。
        std::uint64_t generation;
        /// 待导入文件的本地路径；空值表示教学数据请求。
        QString path;
    };
    /// Worker写入、线程完成后GUI读取的任务结果；通过线程完成/等待建立读取时序。
    struct Result {
        /// 任务持有的共享只读快照，没有跨线程写入口。
        Snapshot snapshot;
        /// 后台捕获的错误文本，GUI在任务完成后读取展示。
        QString error;
        /// 任务因协作取消退出，与格式/读取错误区分。
        bool cancelled = false;
        /// 本次加载耗时（毫秒），成功发布后才成为会话指标。
        double loadMs = 0;
    };
    /// 启动加载Worker并连接GUI完成回调；只有当前代次成功结果才替换会话和来源路径。
    void start(Request request);
    /// 当前Worker，协调器负责退出等待，完成后在GUI线程释放。
    QThread* worker_ = nullptr;
    /// GUI线程进度轮询定时器，合并Worker高频进度变化。
    QTimer poll_;
    /// 共享原子取消标志，仅请求协作退出，不强制终止线程。
    CancelFlag cancel_;
    /// 跨线程进度邮箱，Worker写入、GUI轮询读取。
    std::shared_ptr<ProgressMailbox> progress_;
    /// 唯一待执行请求，新请求替换旧待办以避免无界排队。
    LatestRequest<Request> pending_;
    /// 最近成功发布的只读会话，失败/取消时继续保留。
    Snapshot current_;
    /// 当前请求代次，递增使所有旧任务完成结果失效。
    std::uint64_t generation_ = 0;
    /// 上次已显示的进度，避免重复发相同值。
    int lastProgress_ = -1;
    /// 最近成功加载耗时（毫秒），供状态栏读取。
    double loadMs_ = 0;
    /// 成功导入的本地路径，仅用于防止导出覆盖当前输入。
    QString sourcePath_;
};
}
