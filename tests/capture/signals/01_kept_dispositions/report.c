// Prints the signals it started with.
#include <signal.h>
#include <stdio.h>

static const char* action(int signum) {
    struct sigaction current;
    sigaction(signum, NULL, &current);
    return current.sa_handler == SIG_IGN ? "ignored" : "default";
}

int main(void) {
    sigset_t blocked;
    sigprocmask(SIG_BLOCK, NULL, &blocked);
    printf("SIGHUP %s, SIGPIPE %s, SIGCHLD %s, SIGUSR1 %s\n",
           action(SIGHUP),
           action(SIGPIPE),
           action(SIGCHLD),
           sigismember(&blocked, SIGUSR1) ? "blocked" : "not blocked");
    return 0;
}
