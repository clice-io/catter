module;

#ifndef CATTER_E2E
#error "CATTER_E2E must be defined by the build system"
#endif

export module greet;

export int greet() {
    return 42;
}
