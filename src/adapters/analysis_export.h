/// @file src/adapters/analysis_export.h
/// @brief 将冻结帧分析序列化为CSV/Markdown，并以原子文件替换保证失败或取消不破坏旧报告。
#pragma once
#include "core/frame_analysis.h"
#include <QIODevice>
#include <QString>
namespace gpuview {
/// 导出格式，不改变分析快照中的数据与统计口径。
enum class ExportFormat {
    SummaryCsv, ///< CSV元数据与统计，不含逐帧行。
    Markdown,   ///< 便于阅读的Markdown统计报告。
    FramesCsv   ///< 元数据与当前选区全部帧行，不受快捷列表上限限制。
};
/// 把冻结分析写入已打开设备；UTF-8输出，支持取消/进度，写失败抛异常；不负责提交目标文件。
void writeAnalysis(QIODevice& output, const FrameAnalysis& analysis, ExportFormat format,
    const QString& notes, const CancelFlag& cancel = {}, const Progress& progress = {});
/// 原子保存分析到path；commit前取消或失败保留旧文件，提交成功后报告才可见。
void saveAnalysis(const QString& path, const FrameAnalysis& analysis, ExportFormat format,
    const QString& notes, const CancelFlag& cancel = {}, const Progress& progress = {});
}
