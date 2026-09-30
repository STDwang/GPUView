/// @file theme.h
/// @brief 集中维护Widgets视觉状态，避免窗口构造函数堆积样式字符串。
#pragma once
#include <QString>
namespace gpuview {
/// 返回统一深色主题；逻辑像素由Qt处理DPI缩放，不烘焙位图控件。
QString darkTheme();
}
