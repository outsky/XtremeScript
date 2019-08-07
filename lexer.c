#include "lexer.h"

void _freetoken(L_Token *t) {
    if (t->type == L_TT_STRING) {
        free(t->u.s); t->u.s = NULL;
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
    free(Ls->source);
    _freetoken(&Ls->curtoken);

    free(Ls);
}

enum _LexState {
    L_LS_INIT;
    L_LS_INT;       // 123
    L_LS_INTDOT;    // 123.
    L_LS_FLOAT;     // 123.123
    L_LS_IDENT;
    L_LS_STRHALF;   // "asf
    L_LS_STRTRANS;  // "sdf\
};

L_Token* L_nexttoken(L_State *Ls) {
    if (Ls->cached) {
        Ls->cached = 0;
        return &Ls->curtoken;
    }

    if (Ls->curidx >= strlen(Ls->source)) {
        return L_TT_EOT;
    }

    _LexState *ls = L_LS_INIT;
    long begin = 0;
    for (;;) {
        char c = Ls->source[Ls->curidx++];
        switch (ls) {
            case L_LS_INIT: {
                if (c == '\n') {
                    ++Ls->curline;
                    break;
                }
                if (isblank(c)) {
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

            } break;
            case L_LS_INT: {} break;
            case L_LS_INTDOT: {} break;
            case L_LS_FLOAT: {} break;
            case L_LS_IDENT: {} break;
            case L_LS_STRHALF: {} break;
            case L_LS_STRTRANS: {} break;
        }
    }
}

void L_cachenexttoken(L_State *Ls) {
    Ls->cached = 1;
}


