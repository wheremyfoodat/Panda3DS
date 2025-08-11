#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    // Put the string on the heap to make sure heap initialization works
    char* buffer = calloc(6969, sizeof(char));
    strcpy(buffer, "Hello world\n");

    svcOutputDebugString(buffer, strlen(buffer));

    // I hate C's UB rules around infinite loops
    while (true) {
        __asm__ volatile ("" ::: "memory");
    }

    return 0;
}
