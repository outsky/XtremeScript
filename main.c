#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xasm.h"
#include "xvm.h"
#include "lexer.h"
#include "parser.h"
#include "emitter.h"

void usage(const char *pname) {
    printf("%s [-op] filename\n", pname);
    printf("op:\n"
            "\tc: compile .xasm to .xse\n"
            "\tr: run .xse\n"
            "\tl: lexer .xss\n"
            "\tp: parse .xss\n"
            "\te: comile .xss to .xasm\n"
    );
}

void compile_xasm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *source = (char *)malloc(fsize + 1);
    memset(source, 0, fsize + 1);
    fread(source, sizeof(char), fsize, f);
    fclose(f); f = NULL;

    A_State *As = A_newstate(source);
    free(source);

    A_parse(As);
    A_createbin(As);

    A_freestate(As);
}

void run_xse(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }

    V_State *Vs = V_newstate();
    V_loadapis(Vs);
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

void lexer_xss(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *source = (char *)malloc(fsize + 1);
    memset(source, 0, fsize + 1);
    fread(source, sizeof(char), fsize, f);
    fclose(f); f = NULL;

    L_State *Ls = L_newstate(source);
    free(source);

    int curline = Ls->curline;
    for (;;) {
        if (L_nexttoken(Ls) == L_TT_EOT) {
            printf("<EOT>\n");
            break;
        }
        if (curline != Ls->curline) {
            curline = Ls->curline;
            printf("\n");
        }
        L_printtoken(&Ls->curtoken);
        printf(" ");
    }

    L_freestate(Ls);
}

void parse_xss(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *source = (char *)malloc(fsize + 1);
    memset(source, 0, fsize + 1);
    fread(source, sizeof(char), fsize, f);
    fclose(f); f = NULL;

    L_State *Ls = L_newstate(source);
    free(source);

    P_State *ps = P_newstate(Ls);
    P_parse(ps);

    P_freestate(ps);
}

void emit_xasm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        perror("Open file failed");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *source = (char *)malloc(fsize + 1);
    memset(source, 0, fsize + 1);
    fread(source, sizeof(char), fsize, f);
    fclose(f); f = NULL;

    L_State *Ls = L_newstate(source);
    free(source);

    P_State *ps = P_newstate(Ls);
    P_parse(ps);

    FILE *fout = fopen("out.xasm", "w");
    if (fout == NULL) {
        P_freestate(ps);
        perror("Open out.xasm failed");
        exit(-1);
    }
    E_emit(ps, fout);
    fclose(fout); fout = NULL;

    P_freestate(ps);
}

void directrun(const char *filename) {
    emit_xasm(filename);
    compile_xasm("out.xasm");
    run_xse("a.xse");
}

int main(int argc, const char **argv) {
    const char* pname = argv[0];
    if (argc == 2) {
        directrun(argv[1]);
        return 0;
    }
    if (argc != 3) {
        usage(pname);
        exit(-1);
    }

    const char *opt = argv[1];
    const char *filename = argv[2];
    if (strcmp(opt, "-c") == 0) {
        compile_xasm(filename);
    } else if (strcmp(opt, "-r") == 0) {
        run_xse(filename);
    } else if (strcmp(opt, "-l") == 0) {
        lexer_xss(filename);
    } else if (strcmp(opt, "-p") == 0) {
        parse_xss(filename);
    } else if (strcmp(opt, "-e") == 0) {
        emit_xasm(filename);
    } else {
        usage(pname);
        exit(-1);
    }

    return 0;
}
