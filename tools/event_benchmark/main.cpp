/// @file tools/event_benchmark/main.cpp
/// @brief 教学100k/1m事件搜索与名称汇总的Release基准；分别验证全量及Kernel子串结果。
#include "core/event_analysis.h"
#include "adapters/synthetic_source.h"
#include <chrono>
#include <fstream>
#include <iostream>
/// argv[1]为原始CSV输出路径；预热一次后测三次，正确性不符返回非零。
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    std::ofstream file(argv[1]); if(!file) return 2;
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
