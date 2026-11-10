#pragma once
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <string>
#include <vector>
namespace camber {
inline std::wstring TokenUserSid(HANDLE token) {
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);
    if(!size)return {};
    std::vector<unsigned char> bytes(size);
    if(!GetTokenInformation(token,TokenUser,bytes.data(),size,&size))return {};
    LPWSTR text=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&text))return {};
    std::wstring value=text;LocalFree(text);return value;
}
inline std::wstring CurrentUserSid() {
    HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};
    auto value=TokenUserSid(token);CloseHandle(token);return value;
}
// This is a consistency check, NOT proof of camera-registration ownership.
// Unimpersonated Frame Server metadata activation has a service identity rather
// than the requesting user's identity. That separate trust boundary remains a
// cannot authorize pixels. FrameProofGrant is the separate mandatory output gate.
inline bool OwnerConsistentWithActivation(const std::wstring& owner) {
    if(owner.empty())return false;
    HANDLE token=nullptr;
    if(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&token)) {
        const auto effective=TokenUserSid(token);CloseHandle(token);
        return !effective.empty() && effective==owner;
    }
    if(GetLastError()!=ERROR_NO_TOKEN)return false;
    const auto process=CurrentUserSid();
    return !process.empty() && (process==owner || process==L"S-1-5-19" || process==L"S-1-5-18");
}
// Existing named objects do not adopt the requested creation ACL. Validate them
// before mapping any pixels; never trust a name or successful Open alone.
inline bool RestrictedObject(HANDLE object,const std::wstring& ownerSid) {
    PSID user=nullptr,system=nullptr,service=nullptr;
    if(!ConvertStringSidToSidW(ownerSid.c_str(),&user))return false;
    ConvertStringSidToSidW(L"S-1-5-18",&system);ConvertStringSidToSidW(L"S-1-5-19",&service);
    PSECURITY_DESCRIPTOR descriptor=nullptr;PSID owner=nullptr;PACL acl=nullptr;
    bool valid=false;
    if(system&&service&&GetSecurityInfo(object,SE_KERNEL_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&descriptor)==ERROR_SUCCESS&&owner&&acl) {
        auto allowed=[&](PSID sid){return IsValidSid(sid)&&(EqualSid(sid,user)||EqualSid(sid,system)||EqualSid(sid,service));};
        valid=allowed(owner)&&acl->AceCount>0;
        for(DWORD i=0;valid&&i<acl->AceCount;i++) {
            void* entry=nullptr;if(!GetAce(acl,i,&entry)){valid=false;break;}
            auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(entry);
            if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE||!allowed(&ace->SidStart))valid=false;
        }
    }
    if(descriptor)LocalFree(descriptor);if(user)LocalFree(user);if(system)LocalFree(system);if(service)LocalFree(service);return valid;
}
}
