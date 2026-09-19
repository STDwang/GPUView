/// @file src/ui_widgets/main_window.h
/// @brief Widgets应用组装入口；连接导航、时间轴、统计与导出，不直接解析文件或跑重计算。
#pragma once
#include "application/session_controller.h"
#include "application/statistics_controller.h"
#include "application/export_controller.h"
#include "ui_widgets/timeline_widget.h"
#include <QMainWindow>
namespace gpuview {
/// 界面装配及任务协调窗口，控制器为值成员，子控件按Qt父子关系释放。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// 组装Dock、工具栏及连接；只协调界面，解析与大规模计算交给后台模块。
    explicit MainWindow(QWidget* parent = nullptr);
    /// 返回窗口内控制器的非拥有指针，供入口和测试发起加载。
    SessionController* controller() { return &controller_; }
    /// 返回窗口拥有的时间轴访问指针，调用方不得删除或在窗口销毁后使用。
    TimelineWidget* timeline() const { return timeline_; }
private:
    /// 窗口内加载协调器，负责快照发布并在析构时等待Worker。
    SessionController controller_;
    /// 窗口内统计协调器，负责范围/排序请求和迟到结果过滤。
    StatisticsController statistics_;
    /// 窗口内导出协调器，冻结报告快照并负责取消和退出等待。
    ExportController exports_;
    /// 窗口拥有的时间轴访问指针，Qt父子关系负责释放。
    TimelineWidget* timeline_ = nullptr;
};
}
