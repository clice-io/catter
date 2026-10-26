// Starts the file its second argument names with the function its first
// names, and prints whether it started and how it ended.
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

static int spawned(int error, pid_t pid) {
    if(error != 0) {
        printf("failed to start: %s\n", strerror(error));
        return 0;
    }
    int status;
    waitpid(pid, &status, 0);
    printf("exit %d\n", WEXITSTATUS(status));
    return 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* args[] = {argv[2], NULL};
    pid_t pid;
    if(strcmp(argv[1], "posix_spawn") == 0) {
        return spawned(posix_spawn(&pid, argv[2], NULL, NULL, args, environ), pid);
    }
    if(strcmp(argv[1], "posix_spawnp") == 0) {
        return spawned(posix_spawnp(&pid, argv[2], NULL, NULL, args, environ), pid);
    }
    if(strcmp(argv[1], "execv") == 0) {
        execv(argv[2], args);
    } else {
        execvp(argv[2], args);
    }
    printf("failed to start: %s\n", strerror(errno));
    return 0;
}
