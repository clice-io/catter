#include "util.h"

#ifndef CATTER_E2E
#error "CATTER_E2E must be defined by the build system"
#endif

int main() {
    return answer() == 42 ? 0 : 1;
}
