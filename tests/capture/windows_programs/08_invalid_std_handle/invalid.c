// invalid.exe: starts itself as a child whose standard output is a handle
// value the child does not have, as a program passes one it did not let the
// child inherit, and prints the child's exit code. invalid.exe child: exits 0.
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(int argc, char** argv) {
    if(argc > 1) {
        return 0;
    }

    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wchar_t line[MAX_PATH + 16];
    swprintf(line, MAX_PATH + 16, L"\"%ls\" child", self);

    STARTUPINFOW startup = {sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    // No process has this many handles.
    startup.hStdOutput = (HANDLE)(ULONG_PTR)0x3FFFFFFC;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION info;
    if(!CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info)) {
        return 3;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    printf("child exit %lu\n", code);
    return 0;
}
