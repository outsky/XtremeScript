#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lib.h"
#include "lexer.h"

typedef enum {
    L_LS_INIT,
    L_LS_INT,       // 123
    L_LS_INTDOT,    // 123.
    L_LS_FLOAT,     // 123.123
    L_LS_IDENT,
    L_LS_STRHALF,   // "asf
    L_LS_STRTRANS,  // "sdf\?
} _LexState;

typedef struct {
    char op1;
    L_TokenType t1;
    char op2_1;
    L_TokenType t2_1;
    char op2_2;
    L_TokenType t2_2;
    char op3;
    L_TokenType t3;
    L_TokenType t3_pre;
} _OpTrans;

_OpTrans _opcfg[] = {
    /*
     op1  t1                op2_1 t2_1                op2_2 t2_2                op3  t3                     t3_pre
    */
    {'=', L_TT_OP_ASS,      '=',  L_TT_OP_LOG_EQ,     0,    0,                  0,   0,                     0},
    {'+', L_TT_OP_ADD,      '+',  L_TT_OP_INC,        '=',  L_TT_OP_ADDASS,     0,   0,                     0},
    {'-', L_TT_OP_SUB,      '-',  L_TT_OP_DEC,        '=',  L_TT_OP_SUBASS,     0,   0,                     0},
    {'*', L_TT_OP_MUL,      '=',  L_TT_OP_MULASS,     0,    0,                  0,   0,                     0},
    {'/', L_TT_OP_DIV,      '=',  L_TT_OP_DIVASS,     0,    0,                  0,   0,                     0},
    {'%', L_TT_OP_MOD,      '=',  L_TT_OP_MODASS,     0,    0,                  0,   0,                     0},
    {'^', L_TT_OP_EXP,      '=',  L_TT_OP_EXPASS,     0,    0,                  0,   0,                     0},
    {'$', L_TT_OP_CONCAT,   0,    0,                  0,    0,                  0,   0,                     0},
    {'&', L_TT_OP_BIT_AND,  '&',  L_TT_OP_LOG_AND,    '=',  L_TT_OP_BIT_ANDASS, 0,   0,                     0},
    {'|', L_TT_OP_BIT_OR,   '|',  L_TT_OP_LOG_OR,     '=',  L_TT_OP_BIT_ORASS,  0,   0,                     0},
    {'#', L_TT_OP_BIT_XOR,  '=',  L_TT_OP_BIT_XORASS, 0,    0,                  0,   0,                     0},
    {'~', L_TT_OP_BIT_NOT,  0,    0,                  0,    0,                  0,   0,                     0},
    {'<', L_TT_OP_L,        '<',  L_TT_OP_BIT_SLEFT,  '=',  L_TT_OP_LE,         '=', L_TT_OP_BIT_SLEFTASS,  L_TT_OP_BIT_SLEFT},
    {'>', L_TT_OP_G,        '>',  L_TT_OP_BIT_SRIGHT, '=',  L_TT_OP_GE,         '=', L_TT_OP_BIT_SRIGHTASS, L_TT_OP_BIT_SRIGHT},
    {'!', L_TT_OP_LOG_NOT,  '=',  L_TT_OP_LOG_NEQ,    0,    0,                  0,   0,                     0},
    {0,   0,                0,    0,                  0,    0,                  0,   0,                     0},
};

#define L_FATAL(msg) printf("<line: %d>\n", Ls->curline); snapshot(Ls->source, Ls->curidx); fatal(__FUNCTION__, __LINE__, msg)

static char _peek(L_State *Ls, int forward);

static int _isnumterminal(char c);
static int _isidterminal(char c);

static const _OpTrans* _getopcfg(char op);

static char _peek(L_State *Ls, int forward) {
    long idx = Ls->curidx + forward;
    if (idx >= strlen(Ls->source)) {
        return '\0';
    }
    return *(Ls->source + idx);
}

static int _isnumterminal(char c) {
    return isspace(c) || c == ';' || c == ')' || c == ']' || c == ',' || 
        c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^' ||
        c == '&' || c == '|' || c == '#' || c == '~' || c == '<' || c == '>' ||
        c == '!' || c == '=';
}
static int _isidterminal(char c) {
    return _isnumterminal(c) || c == '(' || c == '[' || c == '{' || c == '}' || 
        c == '$' || c == '#';
}
static const _OpTrans* _getopcfg(char op) {
    int i = 0;
    for (;;) {
        const _OpTrans *cfg = &_opcfg[i++];
        if (cfg->op1 == 0) {
            return NULL;
        }
        if (cfg->op1 == op) {
            return cfg;
        }
    }

}
void _freetoken(L_Token *t) {
    if (t->type == L_TT_STRING || t->type == L_TT_IDENT) {
        free((void*)t->u.s); t->u.s = NULL;
    }
    t->type = L_TT_INVALID;
}

L_State* L_newstate(const char *source) {
    L_State *Ls = (L_State*)malloc(sizeof(*Ls));
    memset(Ls, 0, sizeof(*Ls));
    Ls->source = strdup(source);
    Ls->curline = 1;
    Ls->curtoken.type = L_TT_INVALID;

    return Ls;
}

void L_freestate(L_State *Ls) {
    free((void*)Ls->source);
    _freetoken(&Ls->curtoken);

    free(Ls);
}

L_TokenType L_nexttoken(L_State *Ls) {
    if (Ls->cached) {
        Ls->cached = 0;
        return Ls->curtoken.type;
    }

    _freetoken(&Ls->curtoken);
    _LexState ls = L_LS_INIT;
    long begin = 0;
    for (;;) {
        if (Ls->curidx >= strlen(Ls->source)) {
            Ls->curtoken.type = L_TT_EOT;
            return L_TT_EOT;
        }
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
                if (c == ';') {Ls->curtoken.type = L_TT_SEM; return Ls->curtoken.type;}
                if (c == '(') {Ls->curtoken.type = L_TT_OPEN_PAR; return Ls->curtoken.type;}
                if (c == ')') {Ls->curtoken.type = L_TT_CLOSE_PAR; return Ls->curtoken.type;}
                if (c == '[') {Ls->curtoken.type = L_TT_OPEN_BRACKET; return Ls->curtoken.type;}
                if (c == ']') {Ls->curtoken.type = L_TT_CLOSE_BRACKET; return Ls->curtoken.type;}
                if (c == ',') {Ls->curtoken.type = L_TT_COMMA; return Ls->curtoken.type;}
                if (c == '{') {Ls->curtoken.type = L_TT_OPEN_BRACE; return Ls->curtoken.type;}
                if (c == '}') {Ls->curtoken.type = L_TT_CLOSE_BRACE; return Ls->curtoken.type;}

                const _OpTrans *opcfg = _getopcfg(c);
                if (opcfg != NULL) {
                    --Ls->curidx;
                    char n1 = _peek(Ls, 1);
                    char n2 = _peek(Ls, 2);
                    ++Ls->curidx;

                    L_TokenType type = opcfg->t1;
                    if (n1 && n1 == opcfg->op2_1) {
                        type = opcfg->t2_1;
                    } else if (n1 && n1 == opcfg->op2_2) {
                        type = opcfg->t2_2;
                    } else {
                        Ls->curtoken.type = type;
                        return type;
                    }

                    if (n2 && n2 == opcfg->op3 && type == opcfg->t3_pre) {
                        Ls->curidx += 2;
                        Ls->curtoken.type = opcfg->t3;
                        return Ls->curtoken.type;
                    } else {
                        ++Ls->curidx;
                        Ls->curtoken.type = type;
                        return type;
                    }
                }
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
                if (_isnumterminal(c)) {
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
                if (_isnumterminal(c)) {
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
                if (_isidterminal(c)) {
                    const char *tmp = strndup(Ls->source + begin, Ls->curidx - begin - 1);
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

