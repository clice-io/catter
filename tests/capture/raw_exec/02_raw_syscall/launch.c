/* A program the hook loads into starts a program with the exec system call
 * itself, as Go's runtime does, which the hook never sees:
 * `launch fork <program> <args>...` in a child of its own, waiting for it,
 * `launch exec <program> <args>...` in its own place. */
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char* argv[]) {
    if(argc < 3) {
        return 2;
    }
    if(strcmp(argv[1], "exec") == 0) {
        syscall(SYS_execve, argv[2], argv + 2, environ);
        return 127;
    }
    pid_t pid = fork();
    if(pid == 0) {
        syscall(SYS_execve, argv[2], argv + 2, environ);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
