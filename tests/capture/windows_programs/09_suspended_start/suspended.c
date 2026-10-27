// suspended.exe: starts itself as a child, suspended, as a tool that prepares
// its child before it runs does (MSBuild's tracker, a debugger): prints the
// program the process it got runs, how many times its thread is suspended,
// and whether it could write into it, then resumes it and prints its exit
// code. suspended.exe child: prints that it runs and exits 7.
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(int argc, char** argv) {
    if(argc > 1) {
        printf("child runs\n");
        return 7;
    }

    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wchar_t line[MAX_PATH + 16];
    swprintf(line, MAX_PATH + 16, L"\"%ls\" child", self);

    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION info;
    if(!CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup,
                       &info)) {
        return 3;
    }
    wchar_t image[MAX_PATH];
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameW(info.hProcess, 0, image, &size);
    const wchar_t* name = wcsrchr(image, L'\\');
    printf("suspended: %ls\n", name != NULL ? name + 1 : image);
    printf("suspend count: %lu\n", SuspendThread(info.hThread));
    ResumeThread(info.hThread);
    const char mark[] = "prepared";
    void* page =
        VirtualAllocEx(info.hProcess, NULL, sizeof(mark), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    const BOOL written =
        page != NULL && WriteProcessMemory(info.hProcess, page, mark, sizeof(mark), NULL);
    printf("written into: %s\n", written ? "yes" : "no");
    fflush(stdout);
    ResumeThread(info.hThread);
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    printf("child exit %lu\n", code);
    return 0;
}
