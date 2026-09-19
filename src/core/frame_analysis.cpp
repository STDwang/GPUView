#include "core/frame_analysis.h"
#include <algorithm>
#include <limits>
#include <cmath>
namespace gpuview {
const Event& FrameAnalysis::event(std::size_t row) const {
    const auto& ref = rows.at(row); return source->tracks.at(ref.track).index.events().at(ref.index);
}
int FrameAnalysis::rowForId(std::uint64_t id) const {
    const auto it = std::lower_bound(idRows.begin(), idRows.end(), id,
        [](const auto& pair, auto value) { return pair.first < value; });
    return it != idRows.end() && it->first == id ? it->second : -1;
}
FrameAnalysisPtr analyzeFrames(Snapshot source, TimeRange range, std::vector<std::uint32_t> tracks,
    FrameSort sort, bool descending, const CancelFlag& cancel) {
    if (!source || !source->frames || tracks.size() > 1) throw std::invalid_argument("single frame group required");
    if (!tracks.empty() && tracks.front() >= source->tracks.size()) throw std::invalid_argument("invalid frame group");
    auto out = std::make_shared<FrameAnalysis>(); out->source = std::move(source); out->range = range;
    out->tracks = std::move(tracks); out->sort = sort; out->descending = descending;
    out->summary = calculateStatistics(*out->source, range, out->tracks, cancel, &out->rows);
    if (out->rows.size() > std::size_t(std::numeric_limits<int>::max())) throw std::runtime_error("table row limit exceeded");
    if (!out->tracks.empty()) {
        const auto& events = out->source->tracks[out->tracks.front()].index.events();
        for (std::size_t i=0; i<events.size(); ++i) {
            if (i%1024==0) checkCancelled(cancel);
            const auto& e=events[i]; const auto begin=e.start/1000000000*1000000000;
            if(out->heat.empty() || out->heat.back().begin!=begin) out->heat.push_back({begin,0,0});
            auto& bin=out->heat.back(); ++bin.count; bin.maximum=std::max(bin.maximum,e.duration);
        }
    }
    constexpr int columns=4096; out->heatOverview.resize(columns,0);
    const auto bounds=out->source->bounds; const double scale=double(columns)/(bounds.end-bounds.begin);
    for(std::size_t i=0;i<out->heat.size();++i) {
        if(i%1024==0) checkCancelled(cancel);
        const auto& bin=out->heat[i]; const auto end=bin.begin+std::min<TimeNs>(1000000000,bounds.end-bin.begin);
        const int first=std::clamp(int((bin.begin-bounds.begin)*scale),0,columns-1);
        const int last=std::clamp(int(std::ceil((end-bounds.begin)*scale))-1,first,columns-1);
        for(int x=first;x<=last;++x) out->heatOverview[std::size_t(x)]=std::max(out->heatOverview[std::size_t(x)],bin.maximum);
    }
    auto key = [&](const FrameRow& row) -> std::uint64_t {
        const auto& e=out->source->tracks[row.track].index.events()[row.index];
        switch(sort) {
        case FrameSort::Id: return e.id;
        case FrameSort::Duration: return std::uint64_t(e.duration);
        case FrameSort::LongFrame: return row.longFrame ? 1 : 0;
        default: return std::uint64_t(e.start);
        }
    };
    std::size_t comparisons=0;
    std::sort(out->rows.begin(),out->rows.end(),[&](const auto& a,const auto& b) {
        if(++comparisons%4096==0) checkCancelled(cancel);
        const auto x=key(a), y=key(b);
        if(x!=y) return descending ? x>y : x<y;
        // 同值总是按稳定ID升序；不依赖上一次排序状态。
        return out->source->tracks[a.track].index.events()[a.index].id < out->source->tracks[b.track].index.events()[b.index].id;
    });
    out->idRows.reserve(out->rows.size());
    for(std::size_t i=0;i<out->rows.size();++i) {
        if(i%1024==0) checkCancelled(cancel);
        out->idRows.emplace_back(out->event(i).id,int(i));
    }
    std::sort(out->idRows.begin(),out->idRows.end(),[&](const auto& a,const auto& b) {
        if(++comparisons%4096==0) checkCancelled(cancel); return a.first<b.first;
    });
    checkCancelled(cancel); return out;
}
}
