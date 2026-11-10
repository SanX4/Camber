#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <ks.h>
#include <ksproxy.h>
#include "../common/frame_proof.h"
#include <wrl.h>
#include <iostream>
#include "../common/guids.h"
#include "../common/object_security.h"
using Microsoft::WRL::ComPtr;
using namespace mycam;
void check(HRESULT hr) {if(FAILED(hr)) throw hr;}
int main() {
    try {
        check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));check(MFStartup(MF_VERSION));
        HMODULE module=LoadLibraryW(L"Camber.Camera.dll");if(!module) return 1;
        auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        if(!getClass) return 1;
        {
            ComPtr<IClassFactory> factory;check(getClass(CLSID_MyCamVCam,IID_PPV_ARGS(&factory)));
            ComPtr<IMFActivate> activate;check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&activate)));
            ComPtr<IMFMediaSource> source;
            if(activate->ActivateObject(IID_PPV_ARGS(&source))!=E_ACCESSDENIED) return 2;
            const auto sid=camber::CurrentUserSid();if(sid.empty())return 6;
            const auto mapping=std::wstring(L"Global\\Camber.")+sid;
            HANDLE prior=OpenFileMappingW(FILE_MAP_READ,FALSE,mapping.c_str());
            const bool hadMapping=prior!=nullptr;if(prior)CloseHandle(prior);
            const auto processService=sid==L"S-1-5-18" || sid==L"S-1-5-19";
            // Local generated-fixture activation; no camera registration or video.
            if(!processService) {
                const wchar_t* foreign=L"S-1-5-21-901293-357189-731991-1001";
                check(activate->SetString(kOwnerAttribute,foreign));
                check(activate->SetString(kMappingAttribute,L"Global\\Camber.S-1-5-21-901293-357189-731991-1001"));
                if(activate->ActivateObject(IID_PPV_ARGS(&source))!=E_ACCESSDENIED)return 7;
            }
            check(activate->SetString(kOwnerAttribute,sid.c_str()));check(activate->SetString(kMappingAttribute,mapping.c_str()));
            check(activate->ActivateObject(IID_PPV_ARGS(&source)));
            ComPtr<IMFPresentationDescriptor> descriptor;check(source->CreatePresentationDescriptor(&descriptor));
            DWORD count=0;check(descriptor->GetStreamDescriptorCount(&count));if(count!=1) return 3;
            HANDLE unexpected=OpenFileMappingW(FILE_MAP_READ,FALSE,mapping.c_str());
            if(unexpected) {CloseHandle(unexpected);if(!hadMapping)return 4;}
            ComPtr<IKsControl> control;check(source.As(&control));
            camber::FrameProofKey key;if(!key.Rotate())return 8;
            KSPROPERTY property{kOwnershipPropertySet,kOwnershipPropertyId,KSPROPERTY_TYPE_GET};
            camber::ProofBytes sentinel{};sentinel.fill(0xA5);const auto unchanged=sentinel;
            ULONG returned=999;
            if(control->KsProperty(&property,sizeof(property),sentinel.data(),32,&returned)!=E_ACCESSDENIED || returned!=0 || sentinel!=unchanged)return 9;
            property.Flags=KSPROPERTY_TYPE_BASICSUPPORT;ULONG access=0;
            check(control->KsProperty(&property,sizeof(property),&access,sizeof(access),&returned));
            if(access!=KSPROPERTY_TYPE_SET || returned!=sizeof(access))return 14;
            if(control->KsProperty(&property,sizeof(property),nullptr,0,&returned)!=HRESULT_FROM_WIN32(ERROR_MORE_DATA) || returned!=sizeof(ULONG))return 15;
            property.Flags=KSPROPERTY_TYPE_SET;
            if(control->KsProperty(&property,sizeof(property),nullptr,32,&returned)!=E_INVALIDARG)return 10;
            if(control->KsProperty(&property,sizeof(property),sentinel.data(),31,&returned)!=E_INVALIDARG)return 11;
            check(control->KsProperty(&property,sizeof(property),const_cast<uint8_t*>(key.Token().data()),32,&returned));
            if(returned!=0)return 12;
            check(source->Shutdown());
            if(control->KsProperty(&property,sizeof(property),sentinel.data(),32,&returned)!=MF_E_SHUTDOWN)return 13;
            control.Reset();source.Reset();check(activate->ShutdownObject());
            check(activate->SetString(kMappingAttribute,L"Global\\Camber.different-owner"));
            if(activate->ActivateObject(IID_PPV_ARGS(&source))!=E_ACCESSDENIED) return 5;
        }
        FreeLibrary(module);MFShutdown();CoUninitialize();
        std::cout<<"PASS own-user metadata activation; foreign direct-user identity and mismatched mapping rejected; no privileged mapping created\n";return 0;
    } catch(HRESULT hr) {std::cerr<<"Activation failed: "<<std::hex<<static_cast<unsigned>(hr)<<"\n";return 1;}
}
