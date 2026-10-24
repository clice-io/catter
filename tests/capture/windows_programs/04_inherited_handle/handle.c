// handle.exe: starts itself as a child with an inheritable pipe whose handle
// it passes on the command line, as some build tools pass a pipe, and prints
// what the child wrote to it. handle.exe <handle>: writes to the handle.
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

int main(int argc, char** argv) {
    if(argc > 1) {
        HANDLE pipe = (HANDLE)(ULONG_PTR)_strtoui64(argv[1], NULL, 10);
        DWORD written = 0;
        return WriteFile(pipe, "through the handle", 18, &written, NULL) ? 0 : 1;
    }

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE read = NULL;
    HANDLE write = NULL;
    if(!CreatePipe(&read, &write, &sa, 0)) {
        return 2;
    }
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);

    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wchar_t line[MAX_PATH + 32];
    swprintf(line, MAX_PATH + 32, L"\"%ls\" %llu", self, (unsigned long long)(ULONG_PTR)write);

    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION info;
    if(!CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info)) {
        return 3;
    }
    CloseHandle(write);
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);

    char buffer[64] = {0};
    DWORD got = 0;
    ReadFile(read, buffer, sizeof(buffer) - 1, &got, NULL);
    printf("through the handle: \"%s\", child exit %lu\n", buffer, code);
    return 0;
}
