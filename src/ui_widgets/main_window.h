#pragma once
#include "application/session_controller.h"
#include "ui_widgets/timeline_widget.h"
#include <QMainWindow>
namespace gpuview {
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    SessionController* controller() { return &controller_; }
    TimelineWidget* timeline() const { return timeline_; }
private:
    SessionController controller_;
    TimelineWidget* timeline_ = nullptr;
};
}
