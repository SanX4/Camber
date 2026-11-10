#include "frame_reader.h"
#include "frame_transform.h"
#include "../common/guids.h"
#include "../common/shared_frame.h"
#include <sddl.h>
#include "../common/object_security.h"
#include <cstring>
#include <string>
namespace mycam {
FrameReader::~FrameReader() {
    ClearOwnerProof();
    if(view_) UnmapViewOfFile(view_); if(mapping_) CloseHandle(mapping_); if(mutex_) CloseHandle(mutex_);
}
bool FrameReader::Configure(IMFAttributes* attributes) {
    if(!ownerSid_.empty() || view_ || mapping_ || mutex_)return false;
    wchar_t sid[256]{}, name[320]{};
    if(!attributes || FAILED(attributes->GetString(kOwnerAttribute,sid,256,nullptr)) ||
        FAILED(attributes->GetString(kMappingAttribute,name,320,nullptr))) return false;
    PSID parsed=nullptr; if(!ConvertStringSidToSidW(sid,&parsed)) return false;
    const bool valid=IsValidSid(parsed); LocalFree(parsed);
    if(!valid || !camber::OwnerConsistentWithActivation(sid) || std::wstring(name)!=mappingPrefix_+sid) return false;
    // Discovery activates this source in the unelevated client. Only validate
    // attributes here; service-only resources belong to streaming startup.
    ownerSid_=sid;mappingName_=name;return true;
}
bool FrameReader::EnsureMapping() {
    if(!camber::OwnerConsistentWithActivation(ownerSid_))return false;
    if(view_) return true;
    if(ownerSid_.empty()||mappingName_.empty()) return false;
    const std::wstring acl=std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;LS)(A;;GA;;;")+ownerSid_+L")";
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)) return false;
    SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};
    mutex_=CreateMutexW(&security,FALSE,(mappingName_+L".lock").c_str());
    mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,kSharedBytes,mappingName_.c_str());
    LocalFree(descriptor);
    if(!mapping_||!mutex_ || !camber::RestrictedObject(mapping_,ownerSid_) || !camber::RestrictedObject(mutex_,ownerSid_)) {if(mapping_) CloseHandle(mapping_);if(mutex_) CloseHandle(mutex_);mapping_=mutex_=nullptr;return false;}
    view_=MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,kSharedBytes);
    if(!view_) {CloseHandle(mapping_);CloseHandle(mutex_);mapping_=mutex_=nullptr;return false;}
    auto result=WaitForSingleObject(mutex_,100);
    if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED) {
        UnmapViewOfFile(view_);CloseHandle(mapping_);CloseHandle(mutex_);
        view_=nullptr;mapping_=mutex_=nullptr;return false;
    }
    auto* h=static_cast<SharedHeader*>(view_);
    if(h->magic!=kSharedMagic || h->version!=kSharedVersion) {ZeroMemory(view_,kSharedBytes);h->magic=kSharedMagic;h->version=kSharedVersion;}
    ReleaseMutex(mutex_);return true;
}
void FrameReader::Render(uint8_t* dst,LONG pitch,UINT32 width,UINT32 height) {
    FillBlackNV12(dst,pitch,width,height); if(!EnsureMapping()) return;
    auto result=WaitForSingleObject(mutex_,0); if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED) return;
    auto* h=static_cast<SharedHeader*>(view_);
    // Service identity/owner attributes alone never authorize live pixels.
    // A copied mapping name or digest cannot satisfy the private bearer proof.
    if(h->magic!=kSharedMagic || h->version!=kSharedVersion || !proof_.Matches(h->ownerProofDigest)) {
        ReleaseMutex(mutex_);
        if(!frame_.empty()){SecureZeroMemory(frame_.data(),frame_.size());frame_.clear();}
        return;
    }
    h->consumerTick=GetTickCount64();
    auto w=h->width,ht=h->height,rotation=h->rotation;
    bool have=h->magic==kSharedMagic&&h->version==kSharedVersion&&h->enabled&&camber::ValidCaptureDimensions(w,ht)&&GetTickCount64()-h->frameTick<1000;
    bool mirror=h->mirror!=0,fill=h->fill!=0;
    if(have) {auto bytes=size_t(w)*ht*3/2;frame_.resize(bytes);memcpy(frame_.data(),FrameData(view_),bytes);lastSeq_=h->seq;}
    ReleaseMutex(mutex_);
    if(have) DrawFittedNV12(frame_.data(),w,ht,rotation,mirror,dst,pitch,width,height,fill);
}
bool FrameReader::HasNewFrame() {
    if(!EnsureMapping())return false;
    auto result=WaitForSingleObject(mutex_,0);if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED)return false;
    auto* h=static_cast<SharedHeader*>(view_);
    const bool fresh=h->magic==kSharedMagic && h->version==kSharedVersion && proof_.Matches(h->ownerProofDigest) && h->seq!=lastSeq_;
    ReleaseMutex(mutex_);return fresh;
}
}
