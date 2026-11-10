#pragma once
#include <cstdint>
namespace camber {
// Landscape and physically rotated buffers share the existing 1080p memory budget.
constexpr uint64_t kCapturePixelBudget=1920ull*1080;
constexpr bool ValidCaptureDimensions(uint32_t width,uint32_t height) {
    return width>=2 && height>=2 && width<=1920 && height<=1920 &&
        !((width|height)&1) && uint64_t(width)*height<=kCapturePixelBudget;
}
}
