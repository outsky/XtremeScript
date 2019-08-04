#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xasm.h"
#include "xvm.h"

void usage(const char* pname) {
    printf("compile: %s -c filename\nrun: %s -r filename\n", pname, pname);
}

void compile(const char* filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
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
}

void run(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }

    V_State *Vs = V_newstate();
    int loaded = V_load(Vs, f);
    if (!loaded) {
        printf("[x] V_load failed\n");
        fclose(f); f = NULL;
        V_freestate(Vs);
        exit(-1);
    }
    fclose(f); f = NULL;
    V_run(Vs);
    V_freestate(Vs);
}

int main(int argc, const char **argv) {
    const char* pname = argv[0];
    if (argc != 3) {
        usage(pname);
        exit(-1);
    }

    const char *opt = argv[1];
    const char *filename = argv[2];
    if (strcmp(opt, "-c") == 0) {
        compile(filename);
    } else if (strcmp(opt, "-r") == 0) {
        run(filename);
    } else {
        usage(pname);
        exit(-1);
    }

    return 0;
}
