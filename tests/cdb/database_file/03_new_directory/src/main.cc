#include "util.h"

#ifndef CATTER_TEST
#error "CATTER_TEST must be defined by the build system"
#endif

int main() {
    return answer() == 42 ? 0 : 1;
}
