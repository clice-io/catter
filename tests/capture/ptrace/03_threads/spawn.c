// Starts `child.mjs <how>` from a thread that is not the main one: `spawn`
// forks there and runs it in the child; `exec` runs it in place of this
// program, from that thread.
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char* how;

static void* start(void* unused) {
    (void)unused;
    if(strcmp(how, "exec") == 0) {
        execlp("node", "node", "child.mjs", how, (char*)0);
        _exit(127);
    }
    pid_t child = fork();
    if(child == 0) {
        execlp("node", "node", "child.mjs", how, (char*)0);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    how = argv[1];
    pthread_t thread;
    pthread_create(&thread, 0, start, 0);
    pthread_join(thread, 0);
    return 0;
}
