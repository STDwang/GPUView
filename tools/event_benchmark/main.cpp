/// @file tools/event_benchmark/main.cpp
/// @brief 教学100k/1m事件搜索与名称汇总的Release基准；分别验证全量及Kernel子串结果。
#include "core/event_analysis.h"
#include "adapters/synthetic_source.h"
#include <chrono>
#include <fstream>
#include <iostream>
namespace {
/// 对照全量重算和单项结果复用；交替测量顺序，计时外逐行校验，避免用变快掩盖错误。
int benchmarkSorting(std::ofstream& file) {
    file<<"source_events,sort,mode,run,analysis_ms,matched,groups\n";
    for(const std::size_t count:{100000,1000000}) {
        const auto source=gpuview::generateTrace(count,1);
        gpuview::EventFilter filter; filter.range=source->bounds;
        for(std::uint32_t track=0;track<source->tracks.size();++track) filter.tracks.push_back(track);
        const auto base=gpuview::analyzeEvents(source,filter);
        for(const bool groupSort:{false,true}) {
            auto changed=filter;
            if(groupSort) { changed.groupColumn=0; changed.groupDescending=false; }
            else { changed.eventColumn=4; changed.eventDescending=true; }
            const auto expected=gpuview::analyzeEvents(source,changed);
            for(int run=-1;run<5;++run) for(int turn=0;turn<2;++turn) {
                const bool reuse=(run+turn+2)%2==0;
                const auto begin=std::chrono::steady_clock::now();
                const auto result=gpuview::analyzeEvents(source,changed,{},reuse?base:gpuview::EventAnalysisPtr{});
                const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                if(result->rows.size()!=count || result->groups.size()!=4 || result->reusedSelection!=reuse ||
                    result->idRows!=expected->idRows) return 6;
                for(std::size_t i=0;i<result->rows.size();++i)
                    if(result->rows[i].event->id!=expected->rows[i].event->id ||
                        result->rows[i].contribution!=expected->rows[i].contribution) return 7;
                for(std::size_t i=0;i<result->groups.size();++i)
                    if(result->groups[i].name!=expected->groups[i].name || result->groups[i].count!=expected->groups[i].count ||
                        result->groups[i].totalMs!=expected->groups[i].totalMs) return 8;
                if(run>=0) file<<count<<','<<(groupSort?"group_name":"event_duration")<<','<<(reuse?"reuse":"cold")
                    <<','<<run<<','<<ms<<','<<result->rows.size()<<','<<result->groups.size()<<'\n';
            }
        }
    }
    file.flush(); return file?0:5;
}
}
/// argv[1]为CSV输出；可选--sort运行排序对照，默认搜索基准；计时均不含UI/防抖。
int main(int argc,char** argv) {
    if(argc!=2 && (argc!=3 || std::string(argv[2])!="--sort")) return 1;
    std::ofstream file(argv[1]); if(!file) return 2;
    if(argc==3) return benchmarkSorting(file);
    file<<"source_events,query,run,analysis_ms,matched,groups\n";
    for(const std::size_t count:{100000,1000000}) {
        const auto source=gpuview::generateTrace(count,1);
        for(const std::string query:{"","Kernel"}) {
            gpuview::EventFilter filter; filter.range=source->bounds; filter.text=query;
            for(std::uint32_t track=0;track<source->tracks.size();++track) filter.tracks.push_back(track);
            for(int run=-1;run<3;++run) {
                const auto begin=std::chrono::steady_clock::now();
                const auto result=gpuview::analyzeEvents(source,filter);
                const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                const auto expected=query.empty()?count:count/2;
                if(result->rows.size()!=expected || result->groups.size()!=(query.empty()?4:2)) return 3;
                std::size_t grouped=0; for(const auto& group:result->groups) grouped+=group.count;
                if(grouped!=expected || result->rowForId(query.empty()?1:2)<0) return 4;
                if(run>=0) file<<count<<','<<(query.empty()?"all":"Kernel")<<','<<run<<','<<ms<<','<<result->rows.size()<<','<<result->groups.size()<<'\n';
            }
        }
    }
    file.flush(); if(!file) return 5;
    std::cout<<"Verified full and filtered event counts, name groups, and stable ID lookup.\n";
    return 0;
}
