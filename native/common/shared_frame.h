#pragma once
#include <windows.h>
#include <cstdint>
#include "frame_proof.h"
#include "frame_limits.h"
namespace mycam {
constexpr uint32_t kSharedMagic = 0x43414D42, kSharedVersion = 2, kHeaderBytes = 4096;
constexpr uint32_t kMaxWidth = 1920, kMaxHeight = 1080;
constexpr uint32_t kSharedBytes = kHeaderBytes + kMaxWidth * kMaxHeight * 3 / 2;
struct SharedHeader {
    uint32_t magic, version;
    volatile LONG seq, enabled;
    uint32_t width, height, rotation, mirror, fill;
    uint64_t frameTick, consumerTick;
    camber::ProofBytes ownerProofDigest{};
};
inline uint8_t* FrameData(void* base) { return static_cast<uint8_t*>(base) + kHeaderBytes; }
}
