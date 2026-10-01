/// @file statistics_view.h
/// @brief 将已完成统计转换为紧凑可阅读摘要；不在展示层扫描事件或重算百分位。
#pragma once
#include "core/statistics.h"
#include <QString>
namespace gpuview {
/// 关键指标优先，空样本显示N/A；范围、计算口径和分布仍保留在可滚动详情中。
QString statisticsHtml(const Statistics& statistics,bool frames,std::optional<TimeRange> selection);
}
