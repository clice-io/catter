/* Writes to memory it has freed, which valgrind's memcheck reports. */
#include <stdlib.h>

int main(void) {
    int* volatile p = malloc(sizeof(int));
    free(p);
    *p = 1;
    return 0;
}
