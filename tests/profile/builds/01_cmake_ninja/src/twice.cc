#include "util.h"

#ifndef CATTER_TEST
#error "CATTER_TEST must be defined by the build system"
#endif

int twice() {
    return answer() * 2;
}
