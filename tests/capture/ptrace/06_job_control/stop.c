// Starts a child that stops itself, as a program stopped by the terminal's
// Ctrl+Z is, and tells whether it ran on before this process continued it.
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int ends[2];
    pipe(ends);
    pid_t child = fork();
    if(child == 0) {
        close(ends[0]);
        raise(SIGSTOP);
        write(ends[1], "x", 1);
        _exit(0);
    }
    close(ends[1]);
    int status = 0;
    waitpid(child, &status, WUNTRACED);
    printf("stopped: %s\n", WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP ? "yes" : "no");
    fcntl(ends[0], F_SETFL, O_NONBLOCK);
    char byte;
    printf("ran while stopped: %s\n", read(ends[0], &byte, 1) == 1 ? "yes" : "no");
    kill(child, SIGCONT);
    fcntl(ends[0], F_SETFL, 0);
    printf("ran once continued: %s\n", read(ends[0], &byte, 1) == 1 ? "yes" : "no");
    waitpid(child, &status, 0);
    printf("exit %d\n", WEXITSTATUS(status));
    return 0;
}
