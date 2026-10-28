/* A static program, which no hook loads into, starts a program as Go and
 * static tools do: `launch fork <program> <args>...` in a child of its own,
 * waiting for it, `launch exec <program> <args>...` in its own place. */
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if(argc < 3) {
        return 2;
    }
    if(strcmp(argv[1], "exec") == 0) {
        execvp(argv[2], argv + 2);
        return 127;
    }
    pid_t pid = fork();
    if(pid == 0) {
        execvp(argv[2], argv + 2);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
