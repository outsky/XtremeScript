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

    A_State *As = A_newstate(source);
    free(source);

    A_parse(As);
    A_createbin(As);

    A_freestate(As);

    return 0;
}
