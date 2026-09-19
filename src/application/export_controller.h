/// @file src/application/export_controller.h
/// @brief GUI线程的导出任务协调器；冻结快照，后台写入，定时进度和协作取消。
#pragma once
#include "adapters/analysis_export.h"
#include <QObject>
#include <QThread>
#include <QTimer>
namespace gpuview {
/// 导出生命周期协调器；成员和信号由GUI线程管理，Worker只访问捕获的快照及共享原子状态。
class ExportController : public QObject {
    Q_OBJECT
public:
    /// 在调用线程创建导出协调器；实际写入由Worker完成，parent遵循QObject所有权。
    explicit ExportController(QObject* parent = nullptr) : QObject(parent) {}
    /// 停止轮询、协作取消并等待Worker，避免线程仍访问已销毁状态。
    ~ExportController() override;
    /// 启动单个冻结快照导出；忙碌、空分析或目标为当前输入时拒绝，返回是否已受理。
    bool request(const QString& path, FrameAnalysisPtr analysis, ExportFormat format,
        const QString& notes, const QString& protectedInput = {});
    /// 返回是否仍持有运行中的Worker；仅由GUI线程查询并据此更新操作状态。
    bool busy() const { return worker_ != nullptr; }
    /// 置位共享取消标志，Worker在检查点停止；不能强杀线程，也不能撤销已提交文件。
    void cancel();
signals:
    /// 通知GUI更新忙闲状态，不用于逐记录进度通知。
    void busyChanged(bool busy);
    /// 发布0到100的定时进度采样，合并高频生产者更新以避免事件队列积压。
    void progressChanged(int percent);
    /// 原子提交成功后发布报告路径；此时接收方才可提示保存完成。
    void completed(const QString& path);
    /// 向GUI发布错误、取消或提示文本；Worker不直接修改控件。
    void message(const QString& text);
private:
    /// 当前Worker，协调器负责退出等待，完成后在GUI线程释放。
    QThread* worker_ = nullptr;
    /// 共享原子取消标志，仅请求协作退出，不强制终止线程。
    CancelFlag cancel_;
    /// GUI线程进度轮询定时器，合并Worker高频进度变化。
    QTimer poll_;
    /// 跨线程进度邮箱，Worker写入、GUI轮询读取。
    std::shared_ptr<std::atomic_int> progress_;
};
}
