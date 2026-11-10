#include <windows.h>
#include <mfapi.h>
#include <wrl.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include "../common/guids.h"
#include "../common/shared_frame.h"
#include "../common/object_security.h"
#include "../virtual-camera/frame_reader.h"
using namespace mycam;
using Microsoft::WRL::ComPtr;
void require(bool value,const char* label){if(!value){std::cerr<<"FAIL "<<label<<"\n";std::exit(1);}}
int main(){
    require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)),"COM");
    require(SUCCEEDED(MFStartup(MF_VERSION)),"Media Foundation");
    {
    const auto prefix=L"Local\\Camber.ProofFixture."+std::to_wstring(GetCurrentProcessId())+L".";
    const auto sid=camber::CurrentUserSid(),name=prefix+sid;
    ComPtr<IMFAttributes> attrs;require(SUCCEEDED(MFCreateAttributes(&attrs,3)),"attributes");
    require(SUCCEEDED(attrs->SetString(kOwnerAttribute,sid.c_str())),"owner");
    require(SUCCEEDED(attrs->SetString(kMappingAttribute,name.c_str())),"mapping");
    FrameReader authorized(prefix),copiedAttributes(prefix);
    require(authorized.Configure(attrs.Get()) && copiedAttributes.Configure(attrs.Get()),"configure copies");
    std::vector<uint8_t> output(24);
    auto black=[&](FrameReader& reader){
        std::fill(output.begin(),output.end(),255);reader.Render(output.data(),4,4,4);
        return std::all_of(output.begin(),output.begin()+16,[](uint8_t v){return v==16;}) &&
            std::all_of(output.begin()+16,output.end(),[](uint8_t v){return v==128;});
    };
    require(black(authorized),"ungranted source starts neutral");
    require(!authorized.Configure(attrs.Get()),"mapping identity cannot change after configuration");
    HANDLE map=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name.c_str());
    HANDLE mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(name+L".lock").c_str());
    require(map && mutex,"fixture handles");
    auto* view=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,kSharedBytes);require(view,"fixture map");
    auto* header=static_cast<SharedHeader*>(view);
    camber::FrameProofKey owner,other;require(owner.Rotate() && other.Rotate(),"RNG");
    auto publish=[&](const camber::FrameProofKey& key,uint8_t y){
        require(WaitForSingleObject(mutex,1000)==WAIT_OBJECT_0,"fixture lock");
        header->magic=kSharedMagic;header->version=kSharedVersion;header->enabled=1;
        header->width=4;header->height=4;header->frameTick=GetTickCount64();header->consumerTick=0;
        header->ownerProofDigest=key.Digest();header->seq++;
        std::fill(FrameData(view),FrameData(view)+16,y);std::fill(FrameData(view)+16,FrameData(view)+24,128);
        ReleaseMutex(mutex);
    };
    publish(owner,180);
    require(black(copiedAttributes) && header->consumerTick==0,"copied owner/map cannot read or claim demand");
    require(!copiedAttributes.HasNewFrame(),"ungranted pacing cannot see fresh frames");
    require(!authorized.SetOwnerProof(nullptr,32) && !authorized.SetOwnerProof(owner.Token().data(),31),"malformed grant");
    camber::ProofBytes zero{};require(!authorized.SetOwnerProof(zero.data(),32),"empty proof denied");
    require(authorized.SetOwnerProof(other.Token().data(),32) && black(authorized),"wrong proof denied");
    require(authorized.SetOwnerProof(owner.Digest().data(),32) && black(authorized),"public digest is not bearer proof");
    require(authorized.SetOwnerProof(owner.Token().data(),32),"owner proof");
    require(authorized.HasNewFrame(),"authorized pacing");
    require(!black(authorized) && output[0]==180 && header->consumerTick!=0,"legitimate generated pixels");
    require(black(copiedAttributes),"another instance still denied");
    publish(other,190);
    require(black(authorized) && header->consumerTick==0,"old proof invalid after rotation");
    require(authorized.SetOwnerProof(other.Token().data(),32) && !black(authorized) && output[0]==190,"fresh proof restores owner only");
    header->width=1080;header->height=1920;header->rotation=0;
    std::fill(FrameData(view),FrameData(view)+1080*1920,190);
    std::fill(FrameData(view)+1080*1920,FrameData(view)+1080*1920*3/2,128);
    require(!black(authorized),"portrait dimensions within the same memory budget are supported");
    header->width=1920;header->height=1920;require(black(authorized),"oversized portrait rejected before copy");
    header->width=0xffffffff;header->height=2;require(black(authorized),"dimension overflow rejected");
    publish(other,190);
    authorized.ClearOwnerProof();require(black(authorized),"clear removes grant");
    require(authorized.SetOwnerProof(other.Token().data(),32),"regrant");
    header->enabled=0;require(black(authorized),"Stop cuts output despite valid proof");
    header->enabled=1;header->ownerProofDigest.fill(0);require(black(authorized),"revoked header denied");
    publish(other,200);header->version=1;require(black(authorized),"legacy header denied");
    owner.Clear();other.Clear();require(!camber::NonzeroProof(owner.Token()) && !camber::NonzeroProof(other.Digest()),"key erasure");
    UnmapViewOfFile(view);CloseHandle(map);CloseHandle(mutex);
    }
    MFShutdown();CoUninitialize();
    std::cout<<"PASS generated live pixels require private owner proof; copied attrs, missing/wrong/hash-as-token, rotation, Stop, legacy version and revoked proof remain neutral\n";
}
