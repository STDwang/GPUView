/// @file time_axis.cpp
/// @brief 小规模标签布局，只度量至多九个字符串，不扫描事件或改变时间视口。
#include "ui_widgets/time_axis.h"
#include <algorithm>
#include <cmath>
namespace gpuview {
/// 采用真实字体宽度试排；端点优先，无法同时容纳两个标签时只显示省略后的起点。
std::vector<TimeAxisTick> timeAxisTicks(TimeRange range,int width,const QFontMetrics& metrics) {
    if(width<=0 || range.begin<0 || range.end<=range.begin) return {};
    const auto span=range.end-range.begin;
    const double unit=span>=10000000000LL?1e9:span>=1000000?1e6:1e3;
    const QString suffix=unit==1e9?" s":unit==1e6?" ms":QStringLiteral(" μs");
    for(int count=8;count>=1;--count) {
        const int precision=std::clamp(int(std::ceil(-std::log10(double(span)/unit/count))),0,6);
        std::vector<TimeAxisTick> ticks; int previousRight=-10; bool fits=true;
        for(int i=0;i<=count;++i) {
            // 先商后乘，避免大纳秒跨度直接乘刻度序号溢出。
            const auto time=range.begin+(span/count)*i+(span%count)*i/count;
            const auto text=QString::number(double(time)/unit,'f',precision)+suffix;
            const int textWidth=metrics.horizontalAdvance(text);
            const int x=int(qint64(width)*i/count);
            const int left=std::clamp(x-textWidth/2,0,std::max(0,width-textWidth));
            if(textWidth>width || left<previousRight+10) { fits=false; break; }
            ticks.push_back({x,left,text}); previousRight=left+textWidth;
        }
        if(fits) return ticks;
    }
    return {{0,0,metrics.elidedText(QString::number(double(range.begin)/unit,'f',3)+suffix,Qt::ElideRight,width)}};
}
}
