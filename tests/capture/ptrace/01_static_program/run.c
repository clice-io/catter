// Compiles a.c as a build tool does, forking and then running cc, through
// the C library linked into this static program: the dynamic loader, which
// preloads catter's hook, never runs.
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t child = fork();
    if(child == 0) {
        execlp("cc", "cc", "-c", "a.c", (char*)0);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
