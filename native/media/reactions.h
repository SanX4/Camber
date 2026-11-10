#pragma once
#include <cstdint>
#include <vector>
#include <memory>
namespace camber {
class GpuEffects;
enum class Reaction { None=0, Hearts=1, ThumbsUp, ThumbsDown, Balloons, Rain, Confetti, Lasers, Fireworks };
class ReactionAnimator final {
    uintptr_t graphicsToken_=0;
    Reaction reaction_=Reaction::None;
    uint64_t started_=0;
    bool enabled_=true,reducedMotion_=false;
    std::vector<uint8_t> overlay_;
    std::unique_ptr<GpuEffects> gpu_;
    bool gpuFailed_=false;
public:
    explicit ReactionAnimator(bool allowGpu=true);
    ~ReactionAnimator();
    ReactionAnimator(const ReactionAnimator&)=delete;
    ReactionAnimator& operator=(const ReactionAnimator&)=delete;
    const char* Backend() const { return gpuFailed_?"cpu-fallback":gpu_?"d3d11":"not-initialized"; }
    void Settings(bool enabled,bool reducedMotion);
    void Clear();
    bool Trigger(Reaction reaction,uint64_t now);
    void Render(uint8_t* nv12,int width,int height,unsigned rotation,bool mirror,uint64_t now);
};
}
