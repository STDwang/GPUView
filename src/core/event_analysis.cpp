/// @file event_analysis.cpp
/// @brief 对原始事件执行可取消搜索和名称聚合；显示LOD不会改变分析结果。
#include "core/event_analysis.h"
#include <algorithm>
#include <limits>
#include <map>
namespace gpuview {
/// 二分恢复当前排序中的稳定ID，避免跨排序保存行号。
int EventAnalysis::rowForId(std::uint64_t id) const {
    // idRows按ID升序，与事件表的用户排序无关。
    const auto it=std::lower_bound(idRows.begin(),idRows.end(),id,[](const auto& p,auto value){return p.first<value;});
    return it!=idRows.end() && it->first==id ? it->second : -1;
}
/// 所有耗时工作均在调用方Worker执行，源快照从不被修改。
EventAnalysisPtr analyzeEvents(Snapshot source, EventFilter filter, const CancelFlag& cancel) {
    checkCancelled(cancel);
    if(!source || filter.minimum<0 || (filter.maximum && *filter.maximum<filter.minimum) ||
        filter.eventColumn<0 || filter.eventColumn>5 || filter.groupColumn<0 || filter.groupColumn>4)
        throw std::invalid_argument("invalid event analysis filter");
    auto out=std::make_shared<EventAnalysis>(); out->source=std::move(source); out->filter=std::move(filter);
    const auto& f=out->filter;
    std::vector<bool> included(out->source->tracks.size(),false);
    for(auto track:f.tracks) {
        if(track>=included.size()) throw std::invalid_argument("invalid search track");
        included[track]=true; // 重复轨道ID只计一次。
    }
    std::vector<bool> names; names.reserve(out->source->names.size());
    for(std::size_t i=0;i<out->source->names.size();++i) {
        if(i%1024==0) checkCancelled(cancel);
        names.push_back(out->source->names[i].find(f.text)!=std::string::npos);
    }
    std::map<std::string,std::size_t> groupIds;
    std::size_t visited=0;
    for(std::size_t track=0;track<included.size() && f.range.end>f.range.begin;++track) {
        if(!included[track]) continue;
        for(const auto& e:out->source->tracks[track].index.events()) {
            if(++visited%1024==0) checkCancelled(cancel);
            if(e.start>=f.range.end) break;
            if(out->source->frames ? e.start<f.range.begin : e.end()<=f.range.begin) continue;
            if(!names[e.name] || e.duration<f.minimum || (f.maximum && e.duration>*f.maximum)) continue;
            const TimeNs effective=out->source->frames ? e.duration :
                std::min(e.end(),f.range.end)-std::max(e.start,f.range.begin);
            out->rows.push_back({&e,effective});
            const auto& name=out->source->names[e.name];
            const auto inserted=groupIds.emplace(name,out->groups.size());
            if(inserted.second) out->groups.push_back({name,0,0,0,nullptr});
            auto& group=out->groups[inserted.first->second];
            ++group.count; group.totalMs+=static_cast<long double>(effective)/1e6L;
            if(!group.representative || effective>group.maximum ||
                (effective==group.maximum && e.id<group.representative->id)) {
                group.maximum=effective; group.representative=&e;
            }
        }
    }
    if(out->rows.size()>std::size_t(std::numeric_limits<int>::max()))
        throw std::runtime_error("event table row limit exceeded");
    std::size_t comparisons=0;
    // 比较器按所选列比较原值；同值用稳定ID，保证多次排序可复现。
    std::sort(out->rows.begin(),out->rows.end(),[&](const auto& a,const auto& b) {
        if(++comparisons%4096==0) checkCancelled(cancel);
        const auto& x=*a.event; const auto& y=*b.event; int order=0;
        // 三态比较避免对uint64 ID做有符号减法。
        const auto compare=[](const auto& l,const auto& r){return l<r?-1:l>r?1:0;};
        switch(f.eventColumn) {
        case 0: order=compare(x.id,y.id); break;
        case 1: order=compare(out->source->names[x.name],out->source->names[y.name]); break;
        case 2: order=compare(out->source->tracks[x.track].name,out->source->tracks[y.track].name); break;
        case 3: order=compare(x.start,y.start); break;
        case 4: order=compare(x.duration,y.duration); break;
        default: order=compare(a.contribution,b.contribution); break;
        }
        return order ? (f.eventDescending ? order>0 : order<0) : x.id<y.id;
    });
    // 汇总主键降序不影响同值名称顺序，不使用逐项GUI代理排序。
    std::sort(out->groups.begin(),out->groups.end(),[&](const auto& a,const auto& b) {
        if(++comparisons%4096==0) checkCancelled(cancel);
        if(f.groupColumn==0) return f.groupDescending ? a.name>b.name : a.name<b.name;
        // 次数/纳秒最大值保持整数比较，避免Windows long double精度等同double时相邻大整数合并。
        if(f.groupColumn==1 || f.groupColumn==4) {
            const auto x=f.groupColumn==1?std::uint64_t(a.count):std::uint64_t(a.maximum);
            const auto y=f.groupColumn==1?std::uint64_t(b.count):std::uint64_t(b.maximum);
            return x!=y ? (f.groupDescending ? x>y : x<y) : a.name<b.name;
        }
        // 所有组非空，均值分母保证非零。
        const auto key=[&](const auto& g)->long double {
            return f.groupColumn==2?g.totalMs:g.totalMs/g.count;
        };
        const auto x=key(a),y=key(b);
        return x!=y ? (f.groupDescending ? x>y : x<y) : a.name<b.name;
    });
    out->idRows.reserve(out->rows.size());
    for(std::size_t i=0;i<out->rows.size();++i) {
        if(i%1024==0) checkCancelled(cancel);
        out->idRows.emplace_back(out->rows[i].event->id,int(i));
    }
    // 单独建立ID查找顺序，恢复选择时无需遍历已排序的完整表格。
    std::sort(out->idRows.begin(),out->idRows.end(),[&](const auto& a,const auto& b) {
        if(++comparisons%4096==0) checkCancelled(cancel); return a.first<b.first;
    });
    checkCancelled(cancel); return out;
}
}
