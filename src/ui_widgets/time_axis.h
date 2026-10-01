/// @file time_axis.h
/// @brief 根据真实字体度量安排时间轴标签，避免固定九个标签挤在窄视口。
#pragma once
#include "core/trace_store.h"
#include <QFontMetrics>
#include <QString>
#include <vector>
namespace gpuview {
/// 一个逻辑像素刻度及其标签；坐标相对绘图区左边，不包含轨道名称区。
struct TimeAxisTick {
    /// 网格线横坐标，位于[0,绘图区宽度]。
    int x;
    /// 文本起点横坐标，经过边界夹紧与间距验证。
    int labelLeft;
    /// 带时间单位的标签；极窄视口使用省略号。
    QString text;
};
/// 从最多8个间隔逐步减少，保证标签间至少10逻辑像素；无效范围返回空。
std::vector<TimeAxisTick> timeAxisTicks(TimeRange range,int width,const QFontMetrics& metrics);
}
