#include <stdio.h>
#include <stdlib.h>
#include "lib.h"

void fatal(const char *func, int line, char *msg) {
    printf("[x] %s:%d: %s\n", func, line, msg);
    exit(-1);
}

void info(const char *func, int line, char *msg) {
    printf("%s:%d: %s\n", func, line, msg);
}
