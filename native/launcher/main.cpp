#include <windows.h>
#include <string>
#include <vector>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR arguments,int show) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    std::vector<wchar_t> path(32768);
    DWORD length=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!length || length>=path.size())return 1;
    std::wstring root(path.data(),length);root.resize(root.find_last_of(L"\\/"));
    const auto directory=root+L"\\app",application=directory+L"\\Camber.Desktop.exe";
    if(GetFileAttributesW(application.c_str())==INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(nullptr,L"Camber's application files are missing. Repair Camber using its installer.",L"Camber Camera",MB_OK|MB_ICONERROR);return 2;
    }
    if(std::wstring(arguments)==L"--verify-layout")return 0;
    std::wstring command=L"\""+application+L"\"";
    if(*arguments){command+=L" ";command+=arguments;}
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=WORD(show);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(application.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&process)) {
        MessageBoxW(nullptr,L"Camber could not open. Repair Camber using its installer.",L"Camber Camera",MB_OK|MB_ICONERROR);return 3;
    }
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);return int(code);
}
