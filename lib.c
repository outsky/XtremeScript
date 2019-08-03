#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"

void fatal(const char *func, int line, char *msg) {
    printf("[x] %s:%d: %s\n", func, line, msg);
    exit(-1);
}

void snapshot(const char* code, int pos) {
    int begin, end;
    for (begin = pos - 1; begin > 0; --begin) {
        if (code[begin] == '\n') {
            ++begin;
            break;
        }
    }
    for (end = pos; ; ++end) {
        char c = code[end];
        if (c == '\0' || c == '\n') {
            --end;
            break;
        }
    }
    char *snap = strndup(code + begin, end - begin + 1);
    printf("'''\n%s\n", snap);
    free(snap);
    for (int i = begin; i < end + 1; ++i) {
        printf("%c", i == pos - 1 ? '^' : ' ');
    }
    printf("\n'''\n");
}
