// start.exe <program> [<directory>]: starts <program>, in <directory> when
// given, and prints whether it started, with the error CreateProcess gave,
// and how it ended. start.exe alone exits 0.
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int wmain(int argc, wchar_t** argv) {
    if(argc < 2) {
        return 0;
    }
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION info;
    if(!CreateProcessW(NULL, argv[1], NULL, NULL, FALSE, 0, NULL, argc > 2 ? argv[2] : NULL,
                       &startup, &info)) {
        printf("failed to start: error %lu\n", GetLastError());
        return 0;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    printf("exit %lu\n", code);
    return 0;
}
