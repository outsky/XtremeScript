#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xasm.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("%s filename\n", argv[0]);
        exit(-1);
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("Open file failed");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *source = (char *)malloc(fsize+1);
    memset(source, 0, fsize + 1);
    fread(source, sizeof(char), fsize, f);
    fclose(f); f = NULL;

    A_State *S = A_newstate(source);
    free(source);

    for (;;) {
        A_TokenType t = A_nexttoken(S);
        printf("<%d: ", t);
        if (t == A_TT_EOT) {
            printf("EOT>\n");
            break;
        }
        if (t == A_TT_NEWLINE) {
            printf("NL>\n");
        } else if (t == A_TT_INT) {
            printf("%d> ", S->curtoken.u.n);
        } else if (t == A_TT_FLOAT) {
            printf("%f> ", S->curtoken.u.f);
        } else if (t == A_TT_STRING || t == A_TT_IDENT) {
            printf("%s> ", S->curtoken.u.s);
        } else {
            printf(">");
        }
    }

    A_freestate(S);

    return 0;
}
