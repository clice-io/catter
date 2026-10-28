/* Starts a program the way Java does: through a helper, `launch helper`,
 * which gets an end of a pipe, named by STARTED_FD, that it marks
 * close-on-exec before it execs the program; this process learns the program
 * started when that end closes, and only then writes the program's input. */
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char* argv[]) {
    if(argc > 2 && strcmp(argv[1], "helper") == 0) {
        fcntl(atoi(getenv("STARTED_FD")), F_SETFD, FD_CLOEXEC);
        execvp(argv[2], argv + 2);
        return 127;
    }
    int started[2];
    int input[2];
    if(argc < 2 || pipe(started) != 0 || pipe(input) != 0) {
        return 2;
    }
    char fd[16];
    snprintf(fd, sizeof(fd), "%d", started[1]);
    setenv("STARTED_FD", fd, 1);
    char* args[argc + 2];
    args[0] = argv[0];
    args[1] = "helper";
    for(int i = 1; i <= argc; i += 1) {
        args[i + 1] = argv[i];
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input[0], 0);
    posix_spawn_file_actions_addclose(&actions, input[1]);
    pid_t pid;
    if(posix_spawn(&pid, argv[0], &actions, NULL, args, environ) != 0) {
        return 2;
    }
    close(started[1]);
    close(input[0]);
    char byte;
    while(read(started[0], &byte, 1) > 0) {
    }
    const char message[] = "started\n";
    write(input[1], message, sizeof(message) - 1);
    close(input[1]);
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
