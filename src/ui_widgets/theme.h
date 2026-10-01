/// @file theme.h
/// @brief 集中维护Widgets视觉状态，避免窗口构造函数堆积样式字符串。
#pragma once
#include <QString>
#include <QPalette>
class QApplication;
namespace gpuview {
/// 返回统一深色主题；逻辑像素由Qt处理DPI缩放，不烘焙位图控件。
QString darkTheme();
/// 补全原生箭头、复选框等样式表之外的颜色角色，供应用Fusion样式使用。
QPalette darkPalette();
/// 统一应用与离屏测试的样式、调色板和中文字体；仅读取系统字体，不打包字体资源。
void configureApplicationTheme(QApplication& app);
}
