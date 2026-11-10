#pragma once
#include <array>
#include <algorithm>
namespace camber {
// Three complete one-second windows above the 4 ms p95 limit latch fallback.
// A single slow allocation or transient spike cannot disable a preset.
class ProcessingBudget final {
    std::array<double,30> samples_{};
    unsigned count_=0,bad_=0;
public:
    void Reset(){count_=bad_=0;}
    bool Observe(double milliseconds) {
        samples_[count_++]=milliseconds;
        if(count_<samples_.size())return false;
        auto sorted=samples_;std::sort(sorted.begin(),sorted.end());count_=0;
        bad_=sorted[28]>4.0?bad_+1:0;
        return bad_>=3;
    }
};
}
