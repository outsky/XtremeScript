#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"

void fatal(const char *func, int line, char *msg) {
    printf("[x] %s:%d: %s\n", func, line, msg);
    exit(-1);
}

const char* snapshot(const char* code, int pos, int count) {
    char *s = (char*)malloc((count + 2) * sizeof(*s));
    memset(s, 0, count + 1);
    char *p = s;
    int half = (int)(count / 2);
    int begin = pos > half ? pos - half : 0;
    for (int i = 0; i < count; ++i) {
        char c = code[begin + i];
        if (c == '\0') {
            break;
        }
        *p++ = c;
        if (begin + i == pos) {
            *p++ = '$';
        }
    }
    return s;
}
