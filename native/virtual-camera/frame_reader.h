#pragma once
#include <windows.h>
#include <mfidl.h>
#include <vector>
#include <string>
#include <utility>
#include "../common/frame_proof.h"
namespace mycam {
class FrameReader {
public:
    // Internal resource-name dependency for isolated fixtures. Production uses
    // the default; activation attributes never select this trusted prefix.
    explicit FrameReader(std::wstring prefix=L"Global\\Camber."):mappingPrefix_(std::move(prefix)){}
    FrameReader(const FrameReader&)=delete;
    FrameReader& operator=(const FrameReader&)=delete;
    ~FrameReader();
    bool Configure(IMFAttributes* attributes);
    bool SetOwnerProof(const void* token,size_t bytes){return proof_.Set(token,bytes);}
    void ClearOwnerProof(){proof_.Clear();if(!frame_.empty()){SecureZeroMemory(frame_.data(),frame_.size());frame_.clear();}}
    void Render(uint8_t* dst, LONG pitch, UINT32 width, UINT32 height);
    bool HasNewFrame();
private:
    bool EnsureMapping();
    camber::FrameProofGrant proof_;
    const std::wstring mappingPrefix_;
    std::wstring ownerSid_, mappingName_;
    HANDLE mapping_ = nullptr, mutex_ = nullptr;
    void* view_ = nullptr;
    std::vector<uint8_t> frame_;
    LONG lastSeq_ = -1;
};
}
