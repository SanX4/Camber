#include "../common/object_security.h"
#include <iostream>
#include <vector>
int main(){
 HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 1;
 DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> bytes(size);
 if(!GetTokenInformation(token,TokenUser,bytes.data(),size,&size)){CloseHandle(token);return 2;}CloseHandle(token);
 LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid))return 3;
 std::wstring user=sid;LocalFree(sid);
 if(!camber::OwnerConsistentWithActivation(user))return 7;
 if(user!=L"S-1-5-18" && user!=L"S-1-5-19" && camber::OwnerConsistentWithActivation(L"S-1-5-21-901293-357189-731991-1001"))return 8;
 if(camber::OwnerConsistentWithActivation(L""))return 9;
 // Exercise the thread-token branch using only this process's own identity.
 if(!ImpersonateSelf(SecurityImpersonation))return 10;
 const bool own=camber::OwnerConsistentWithActivation(user);
 const bool foreign=camber::OwnerConsistentWithActivation(L"S-1-5-21-901293-357189-731991-1001");
 if(!RevertToSelf())return 11;
 if(!own || foreign)return 12;
 if(!ImpersonateSelf(SecurityAnonymous))return 13;
 const bool anonymousAllowed=camber::OwnerConsistentWithActivation(user);
 if(!RevertToSelf())return 14;
 if(anonymousAllowed)return 15;
 for(bool broad:{false,true}){
  auto acl=std::wstring(L"O:")+user+L"D:P(A;;GA;;;"+(broad?L"WD":user)+L")";
  PSECURITY_DESCRIPTOR descriptor=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))return 4;
  SECURITY_ATTRIBUTES sa{sizeof(sa),descriptor,FALSE};HANDLE mutex=CreateMutexW(&sa,FALSE,nullptr);LocalFree(descriptor);
  if(!mutex)return 5;bool valid=camber::RestrictedObject(mutex,user);CloseHandle(mutex);
  if(valid==broad)return 6;
 }
 std::cout<<"PASS restricted object accepted; world-accessible object rejected\n";
}
