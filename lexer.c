#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lib.h"
#include "lexer.h"

#define L_FATAL(msg) printf("<line: %d>\n", Ls->curline); snapshot(Ls->source, Ls->curidx); fatal(__FUNCTION__, __LINE__, msg)

static int isnumterminal(char c);
static int isidterminal(char c);

static int isnumterminal(char c) {
    return isspace(c) || c == ';' || c == ')' || c == ']' || c == ',' || 
        c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^' ||
        c == '&' || c == '|' || c == '#' || c == '~' || c == '<' || c == '>' ||
        c == '!' || c == '=';
}
static int isidterminal(char c) {
    return isnumterminal(c) || c == '(' || c == '[' || c == '{' || c == '}' || 
        c == '$' || c == '#';
}

void _freetoken(L_Token *t) {
    if (t->type == L_TT_STRING || t->type == L_TT_IDENT) {
        free((void*)t->u.s); t->u.s = NULL;
    }
    t->type = L_TT_INVALID;
}

L_State* L_newstate(const char *source) {
    L_State *Ls = (L_State*)malloc(sizeof(*Ls));
    Ls->source = strdup(source);
    Ls->curidx = 0;
    Ls->curline = 1;
    Ls->curtoken.type = L_TT_INVALID;

    return Ls;
}

void L_freestate(L_State *Ls) {
    free((void*)Ls->source);
    _freetoken(&Ls->curtoken);

    free(Ls);
}

typedef enum {
    L_LS_INIT,
    L_LS_INT,       // 123
    L_LS_INTDOT,    // 123.
    L_LS_FLOAT,     // 123.123
    L_LS_IDENT,
    L_LS_STRHALF,   // "asf
    L_LS_STRTRANS,  // "sdf\?
} _LexState;

L_TokenType L_nexttoken(L_State *Ls) {
    if (Ls->cached) {
        Ls->cached = 0;
        return L_TT_EOT;
    }

    if (Ls->curidx >= strlen(Ls->source)) {
        return L_TT_EOT;
    }

    _freetoken(&Ls->curtoken);
    _LexState ls = L_LS_INIT;
    long begin = 0;
    for (;;) {
        char c = Ls->source[Ls->curidx++];
        if (c == '\n') {
            ++Ls->curline;
        }
        switch (ls) {
            case L_LS_INIT: {
                if (isspace(c)) {
                    break;
                }
                if (isdigit(c)) {
                    ls = L_LS_INT;
                    begin = Ls->curidx - 1;
                    break;
                }
                if (c == '_' || isalpha(c)) {
                    ls = L_LS_IDENT;
                    begin = Ls->curidx - 1;
                    break;
                }
                if (c == '"') {
                    ls = L_LS_STRHALF;
                    begin = Ls->curidx;
                    break;
                }
                if (c == ';') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_SEM; return Ls->curtoken.type;}
                if (c == '(') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_OPEN_PAR; return Ls->curtoken.type;}
                if (c == ')') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_CLOSE_PAR; return Ls->curtoken.type;}
                if (c == '[') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_OPEN_BRACKET; return Ls->curtoken.type;}
                if (c == ']') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_CLOSE_BRACKET; return Ls->curtoken.type;}
                if (c == ',') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_COMMA; return Ls->curtoken.type;}
                if (c == '{') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_OPEN_BRACE; return Ls->curtoken.type;}
                if (c == '}') {ls = L_LS_INIT; Ls->curtoken.type = L_TT_CLOSE_BRACE; return Ls->curtoken.type;}
                L_FATAL("unexpected char");
            } break;

            case L_LS_INT: {
                if (isdigit(c)) {
                    break;
                }
                if (c == '.') {
                    ls = L_LS_INTDOT;
                    break;
                }
                if (isnumterminal(c)) {
                    ls = L_LS_INIT;
                    const char *tmp = strndup(Ls->source + begin, Ls->curidx - begin);
                    --Ls->curidx;
                    Ls->curtoken.type = L_TT_INT;
                    Ls->curtoken.u.n = atoi(tmp);
                    free((void*)tmp);
                    return Ls->curtoken.type;
                }
                L_FATAL("unexpected char");
            } break;

            case L_LS_INTDOT: {
                if (isdigit(c)) {
                    ls = L_LS_FLOAT;
                    break;
                }
                L_FATAL("unexpected char");
            } break;

            case L_LS_FLOAT: {
                if (isdigit(c)) {
                    break;
                }
                if (isnumterminal(c)) {
                    ls = L_LS_INIT;
                    const char *tmp = strndup(Ls->source + begin, Ls->curidx - begin);
                    --Ls->curidx;
                    Ls->curtoken.type = L_TT_FLOAT;
                    Ls->curtoken.u.f = atof(tmp);
                    free((void*)tmp);
                    return Ls->curtoken.type;
                }
                L_FATAL("unexpected char");
            } break;

            case L_LS_IDENT: {
                if (c == '_' || isalnum(c)) {
                    break;
                }
                if (isidterminal(c)) {
                    ls = L_LS_INIT;
                    const char *tmp = strndup(Ls->source + begin, Ls->curidx - begin);
                    --Ls->curidx;
                    Ls->curtoken.type = L_TT_IDENT;
                    Ls->curtoken.u.s = tmp;
                    return Ls->curtoken.type;
                }
                L_FATAL("unexpected char");
            } break;

            case L_LS_STRHALF: {
                if (c == '\\') {
                    ls = L_LS_STRTRANS;
                    break;
                }
                if (c == '\n') {
                    L_FATAL("unexpected \\n");
                }
                if (c == '"') {
                    ls = L_LS_INIT;
                    Ls->curtoken.type = L_TT_STRING;
                    Ls->curtoken.u.s = strndup(Ls->source + begin, Ls->curidx - begin);
                    return Ls->curtoken.type;
                }
            } break;

            case L_LS_STRTRANS: {
                if (c == '\\' || c == '"') {
                    ls = L_LS_STRHALF;
                    break;
                }
                L_FATAL("unexpected char");
            } break;
        }
    }
}

void L_cachenexttoken(L_State *Ls) {
    Ls->cached = 1;
}

void L_printtoken(const L_Token *t) {
    if (t->type == L_TT_INT) {
        printf("%d", t->u.n);
    } else if (t->type == L_TT_FLOAT) {
        printf("%f", t->u.f);
    } else if (t->type == L_TT_STRING || t->type == L_TT_IDENT) {
        printf("%s", t->u.s);
    } else {
        printf("<%d>", t->type);
    }
}

