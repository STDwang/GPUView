#pragma once
#include "core/frame_analysis.h"
#include <QIODevice>
#include <QString>
namespace gpuview {
enum class ExportFormat { SummaryCsv, Markdown, FramesCsv };
void writeAnalysis(QIODevice& output, const FrameAnalysis& analysis, ExportFormat format,
    const QString& notes, const CancelFlag& cancel = {}, const Progress& progress = {});
void saveAnalysis(const QString& path, const FrameAnalysis& analysis, ExportFormat format,
    const QString& notes, const CancelFlag& cancel = {}, const Progress& progress = {});
}
