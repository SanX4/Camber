#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>
#include <initializer_list>

// An elevated installer must not terminate another user's camera session.
// Check every visible session and fail closed; do not trust an old host's CLI.
bool BlocksUpgrade(const wchar_t* name) {
    for(auto* expected : {L"Camber.Desktop.exe",L"Camber.Host.exe",L"Camber.Media.exe",L"Camber.Tray.exe"})
        if(_wcsicmp(name,expected)==0)return true;
    return false;
}
int wmain(int argc,wchar_t** argv) {
    if(argc==2 && std::wcscmp(argv[1],L"--self-test")==0)
        return BlocksUpgrade(L"camber.host.EXE") && BlocksUpgrade(L"Camber.Desktop.exe") &&
            !BlocksUpgrade(L"Camber.UpgradeCheck.exe") && !BlocksUpgrade(L"Discord.exe") ? 0 : 1;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return 21;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(snapshot,&entry)){CloseHandle(snapshot);return 21;}
    do {if(BlocksUpgrade(entry.szExeFile)){CloseHandle(snapshot);return 20;}}
    while(Process32NextW(snapshot,&entry));
    const auto error=GetLastError();CloseHandle(snapshot);
    return error==ERROR_NO_MORE_FILES ? 0 : 21;
}
