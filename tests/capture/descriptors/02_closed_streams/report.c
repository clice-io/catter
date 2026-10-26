// Prints, on stderr, what its standard streams are.
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

static const char* kind(int fd) {
    struct stat st;
    if(fcntl(fd, F_GETFD) < 0) {
        return "closed";
    }
    fstat(fd, &st);
    if(S_ISREG(st.st_mode)) {
        return "a file";
    }
    if(S_ISFIFO(st.st_mode)) {
        return "a pipe";
    }
    if(S_ISSOCK(st.st_mode)) {
        return "a socket";
    }
    if(S_ISCHR(st.st_mode)) {
        return "a device";
    }
    return "something else";
}

int main(void) {
    fprintf(stderr, "stdin %s, stdout %s, stderr %s\n", kind(0), kind(1), kind(2));
    return 0;
}
