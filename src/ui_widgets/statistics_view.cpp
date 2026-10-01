/// @file statistics_view.cpp
/// @brief 只使用Qt富文本支持的表格和基础样式，避免依赖浏览器或为每项指标创建控件。
#include "ui_widgets/statistics_view.h"
#include <QStringList>
namespace gpuview {
/// 输出无外部资源的富文本，动态内容仅为统计数值；HTML不包含文件路径或用户输入。
QString statisticsHtml(const Statistics& stats,bool frames,std::optional<TimeRange> selection) {
    // 空值与0明确区分；统计内部的默认0不能作为已测得的时长显示。
    const auto metric=[&](double value) { return stats.count?QString::number(value,'f',3):QStringLiteral("N/A"); };
    // 两列四项，在较矮侧栏中优先展示数量、均值及尾延迟，避免先铺满口径说明。
    const auto cell=[](const QString& label,const QString& value) {
        return QStringLiteral("<td width='50%' bgcolor='#203343'><span style='color:#adc1d0;font-size:11px'>%1</span><br><b style='color:#55dabb;font-size:14px'>%2</b></td>").arg(label,value);
    };
    QString html=QStringLiteral("<p style='margin:0;font-size:11px'>%1 · %2</p>")
        .arg(selection?QStringLiteral("选区"):QStringLiteral("全会话")).arg(frames?
        QStringLiteral("帧间隔 ms · 非GPU执行时长"):QStringLiteral("Trace贡献 ms · 非利用率"));
    if(stats.count) html+=QStringLiteral("<table width='100%' cellspacing='2' cellpadding='1'><tr>")+
        cell(QStringLiteral("数量"),QString::number(qulonglong(stats.count)))+cell(QStringLiteral("均值"),metric(stats.meanMs))+
        "</tr><tr>"+cell("P95",metric(stats.p95Ms))+cell("P99",metric(stats.p99Ms))+"</tr></table>";
    else html+=QStringLiteral("<p style='margin:2px'><b>当前范围无有效样本</b><br>数量 0 · 均值/百分位 N/A<br>可清除选区或恢复轨道勾选。</p>");
    html+=selection?QStringLiteral("<p>选区 [%1, %2) ms</p>").arg(double(selection->begin)/1e6,0,'f',3).arg(double(selection->end)/1e6,0,'f',3):
        QStringLiteral("<p>全会话 / 当前显示轨道</p>");
    html+=QStringLiteral("<p>总时长 %1 ms<br>P50 %2 ms</p>").arg(stats.sumMs,0,'f',3).arg(metric(stats.p50Ms));
    if(frames) {
        html+=QStringLiteral("<p>间隔口径FPS %1<br>长帧 %2</p>")
            .arg(stats.count && stats.meanMs>0?QString::number(1000/stats.meanMs,'f',2):QStringLiteral("N/A"))
            .arg(qulonglong(stats.longFrames));
        html+=QStringLiteral("<p>单进程/交换链；按Present时间归属，保留完整帧间隔。长帧使用60FPS预算及历史中位数规则v1。下表仅展示前200个长帧，双击定位；计数不截断。</p>");
    } else html+=QStringLiteral("<p>相交事件；时长裁剪到选区，并发求和可超过墙钟时间。</p>");
    html+=QStringLiteral("<p><b>时长分布 · ms / 数量</b></p><table width='100%' cellspacing='0' cellpadding='2'>");
    const QStringList bins={QStringLiteral("≤8.33"),QStringLiteral("(8.33,16.67]"),QStringLiteral("(16.67,33.33]"),QStringLiteral("(33.33,50]"),QStringLiteral(">50")};
    for(int i=0;i<bins.size();++i) html+=QStringLiteral("<tr><td>%1</td><td align='right'>%2</td></tr>")
        .arg(bins[i].toHtmlEscaped()).arg(qulonglong(std::size_t(i)<stats.histogram.size()?stats.histogram[std::size_t(i)]:0));
    return html+"</table>";
}
}
