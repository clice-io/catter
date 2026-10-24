module;

#ifndef CATTER_TEST
#error "CATTER_TEST must be defined by the build system"
#endif

export module greet;

export int greet() {
    return 42;
}
