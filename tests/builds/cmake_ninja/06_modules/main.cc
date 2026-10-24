import greet;

#ifndef CATTER_TEST
#error "CATTER_TEST must be defined by the build system"
#endif

int main() {
    return greet() == 42 ? 0 : 1;
}
