#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfvirtualcamera.h>
#include <sddl.h>
#include <wrl.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include "../common/guids.h"
using Microsoft::WRL::ComPtr;
using namespace mycam;
void check(HRESULT hr,const char* stage) {
    if(FAILED(hr)) {std::cerr<<stage<<": 0x"<<std::hex<<static_cast<unsigned>(hr)<<"\n";throw hr;}
}
int main(int argc,char** argv) {
    const bool expectLive=argc>1 && std::string(argv[1])=="--expect-live";
    const int samples=argc>2?std::clamp(std::atoi(argv[2]),30,1800):90;
    try {
        check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");check(MFStartup(MF_VERSION),"Media Foundation");
        {
            ComPtr<IMFAttributes> query;check(MFCreateAttributes(&query,1),"Device query");
            check(query->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),"Video query");
            IMFActivate** devices=nullptr;UINT32 count=0;check(MFEnumDeviceSources(query.Get(),&devices,&count),"Enumerate camera sources");
            ComPtr<IMFActivate> selected;
            for(UINT32 i=0;i<count;i++) {
                LPWSTR name=nullptr;UINT32 length=0;
                if(SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,&name,&length))) {
                    if(std::wstring(name).find(kCameraFriendlyName)==0) selected=devices[i];
                    CoTaskMemFree(name);
                }
                devices[i]->Release();
            }
            CoTaskMemFree(devices);
            if(!selected) {std::cerr<<"Camber is not enumerable as a Windows camera.\n";return 4;}
            ComPtr<IMFMediaSource> source;check(selected->ActivateObject(IID_PPV_ARGS(&source)),"Activate Camber source");
            ComPtr<IMFSourceReader> reader;check(MFCreateSourceReaderFromMediaSource(source.Get(),nullptr,&reader),"Create source reader");
            DWORD flags=0,stream=0;LONGLONG time=0;ComPtr<IMFSample> sample;
            for(int i=0;i<5&&!sample;i++) check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&stream,&flags,&time,&sample),"Read virtual frame");
            if(!sample) return 2;
            ComPtr<IMFMediaBuffer> buffer;check(sample->ConvertToContiguousBuffer(&buffer),"Read sample buffer");
            DWORD length=0;check(buffer->GetCurrentLength(&length),"Sample length");
            if(length!=1920*1080*3/2&&length!=1280*720*3/2) return 3;
            std::cout<<"PASS Windows Frame Server delivered a virtual-camera frame ("<<std::dec<<length<<" bytes). No physical camera was opened.\n";
            if(expectLive) {
                // Inspect in memory only: never export pixels or frame hashes.
                // Allow the session lease to grant a newly created source.
                int live=0;bool previousLive=false,readyReported=false;std::unordered_set<uint64_t> distinct;
                for(int i=0;i<samples;i++) {
                    sample.Reset();check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&stream,&flags,&time,&sample),"Read live virtual frame");
                    if(!sample)continue;
                    buffer.Reset();check(sample->ConvertToContiguousBuffer(&buffer),"Live buffer");
                    BYTE* pixels=nullptr;DWORD current=0;check(buffer->Lock(&pixels,nullptr,&current),"Lock live buffer");
                    int changed=0;uint64_t signature=1469598103934665603ull;
                    for(size_t p=0;p<size_t(current)*2/3;p+=128){changed+=pixels[p]!=16;signature=(signature^pixels[p])*1099511628211ull;}
                    buffer->Unlock();bool currentLive=changed>8;
                    if(i==0||currentLive!=previousLive)std::cout<<"OUTPUT tick="<<GetTickCount64()<<" live="<<currentLive<<std::endl;
                    previousLive=currentLive;if(currentLive){live++;distinct.insert(signature);}
                    if(!readyReported && distinct.size()>=2){readyReported=true;std::cout<<"LIVE_READY tick="<<GetTickCount64()<<std::endl;}
                }
                std::cout<<"Observed non-neutral frames: "<<live<<"/"<<samples<<"; distinct non-neutral frames: "<<distinct.size()<<". No images or hashes saved.\n";
                if(live==0 || distinct.size()<2){reader.Reset();source->Shutdown();selected->ShutdownObject();return 5;}
                std::cout<<"PASS approved live pixels reached the registered Windows camera consumer.\n";
            }
            reader.Reset();source->Shutdown();source.Reset();selected->ShutdownObject();
        }
        MFShutdown();CoUninitialize();return 0;
    } catch(HRESULT) {return 1;}
}
