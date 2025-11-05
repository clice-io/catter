#include <new>

/**
 * To avoid undefined symbol from `libc++.so` or `libstdc++.so` define a
 * no-op delete operator. It's safe to not call `free` because we are not
 * `malloc` or `new` in this library.
 *
 * But it's needed since we were declaring a few classes virtual, which
 * requires this symbol to be present.
 */
void operator delete (void*, std::size_t) {}

void operator delete (void*) {}
