/* Starts a program the way build tools do, through the function the first
 * argument names. On POSIX the rest is the program and its arguments (the
 * execl family takes one argument); the exec functions replace this process,
 * the others wait for the program and exit with its status. On Windows the
 * second argument is a file holding the command line CreateProcess gets. */
#ifdef _WIN32
#include <stdio.h>
#include <string.h>
#include <windows.h>

static int wait_for(PROCESS_INFORMATION* info) {
    DWORD code = 1;
    WaitForSingleObject(info->hProcess, INFINITE);
    GetExitCodeProcess(info->hProcess, &code);
    CloseHandle(info->hProcess);
    CloseHandle(info->hThread);
    return (int)code;
}

/* The command line in the file `path` names, UTF-8. */
static wchar_t* read_command_line(const wchar_t* path) {
    static char text[32768];
    static wchar_t line[32768];
    FILE* file = _wfopen(path, L"rb");
    if(file == NULL) {
        return NULL;
    }
    size_t size = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    while(size > 0 && (text[size - 1] == '\n' || text[size - 1] == '\r')) {
        size--;
    }
    text[size] = '\0';
    MultiByteToWideChar(CP_UTF8, 0, text, -1, line, 32768);
    return line;
}

int wmain(int argc, wchar_t** argv) {
    if(argc != 3) {
        fprintf(stderr, "usage: spawn CreateProcessA|CreateProcessW <file of the command line>\n");
        return 2;
    }
    wchar_t* command_line = read_command_line(argv[2]);
    if(command_line == NULL) {
        fprintf(stderr, "spawn: cannot read the command line\n");
        return 2;
    }
    PROCESS_INFORMATION info;
    if(wcscmp(argv[1], L"CreateProcessA") == 0) {
        char line[32768];
        STARTUPINFOA startup = {sizeof(startup)};
        WideCharToMultiByte(CP_ACP, 0, command_line, -1, line, sizeof(line), NULL, NULL);
        if(CreateProcessA(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info)) {
            return wait_for(&info);
        }
    } else if(wcscmp(argv[1], L"CreateProcessW") == 0) {
        STARTUPINFOW startup = {sizeof(startup)};
        if(CreateProcessW(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info)) {
            return wait_for(&info);
        }
    } else {
        fprintf(stderr, "spawn: unknown function\n");
        return 2;
    }
    fprintf(stderr, "spawn: CreateProcess failed with error %lu\n", GetLastError());
    return 127;
}
#else
#define _GNU_SOURCE
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

static int wait_for(int error, pid_t pid) {
    int status = 0;
    if(error != 0) {
        fprintf(stderr, "spawn: %s\n", strerror(error));
        return 127;
    }
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "usage: spawn <function> <program> [arguments...]\n");
        return 2;
    }
    const char* function = argv[1];
    char** command = argv + 2;
    pid_t pid = 0;
    if(strcmp(function, "execv") == 0) {
        execv(command[0], command);
    } else if(strcmp(function, "execve") == 0) {
        execve(command[0], command, environ);
    } else if(strcmp(function, "execvp") == 0) {
        execvp(command[0], command);
#ifdef __linux__
    } else if(strcmp(function, "execvpe") == 0) {
        execvpe(command[0], command, environ);
#endif
    } else if(strcmp(function, "execl") == 0) {
        execl(command[0], command[0], command[1], (char*)NULL);
    } else if(strcmp(function, "execle") == 0) {
        execle(command[0], command[0], command[1], (char*)NULL, environ);
    } else if(strcmp(function, "execlp") == 0) {
        execlp(command[0], command[0], command[1], (char*)NULL);
    } else if(strcmp(function, "posix_spawn") == 0) {
        return wait_for(posix_spawn(&pid, command[0], NULL, NULL, command, environ), pid);
    } else if(strcmp(function, "posix_spawnp") == 0) {
        return wait_for(posix_spawnp(&pid, command[0], NULL, NULL, command, environ), pid);
    } else {
        fprintf(stderr, "spawn: unknown function %s\n", function);
        return 2;
    }
    perror(function);
    return 127;
}
#endif
