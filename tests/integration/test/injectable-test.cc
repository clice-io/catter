// UNSUPPORTED: system-windows
// RUN: %cc
// RUN: %inject-hook1 %t x | FileCheck %s

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// run: ls -a
int main(int argc, const char** argv) {
    if(argc == 1) {
        return -1;
    }
    // call by hook
    if(argv[1][0] != 'x') {
        // just print all args
        for(int i = 1; i < argc; ++i) {
            printf("%s ", argv[i]);
        }
        printf("\n");
        return 0;
    }

    execlp("ls", "ls", "-a", nullptr);
    // CHECK: -p 1 --exec /bin/ls -- ls -a
    return 0;
}
