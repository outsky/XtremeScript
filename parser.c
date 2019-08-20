#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "parser.h"
#include "string.h"

#define P_FATAL(msg) fatal(__FUNCTION__, __LINE__, msg)

static P_Symbol* _get_symbol(P_State *ps, const char *name);
static void _add_symbol(P_State *ps, const char *name, int size, int scope, int isparam);

static int _has_str(P_State *ps, const char *s);
static void _add_str(P_State *ps, const char *s);

static P_Func* _get_func_byidx(P_State *ps, int fidx);
static P_Func* _get_func(P_State *ps, const char *name);
static void _add_func(P_State *ps, const char *name, int param);

static int _has_str(P_State *ps, const char *s) {
    for (lnode *n = ps->strs->head; n != NULL; n = n->next) {
        if (strcmp((char*)n->data, s) == 0) {
            return 1;
        }
    }
    return 0;
}

static void _add_str(P_State *ps, const char *s) {
    if (_has_str(ps, s)) {
        return;
    }
    list_pushback(ps->strs, strdup(s));
    printf("str %d: %s\n", ps->strs->count - 1, s);
}

static P_Symbol* _get_symbol(P_State *ps, const char *name) {
    for (lnode *n = ps->symbols->head; n != NULL; n = n->next) {
        P_Symbol *s = (P_Symbol*)n->data;
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static void _add_symbol(P_State *ps, const char *name, int size, int scope, int isparam) {
    P_Symbol *s = _get_symbol(ps, name);
    if (s && s->scope == scope) {
        P_FATAL("redefined symbol");
    }

    s = (P_Symbol*)malloc(sizeof(*s));
    strcpy(s->name, name);
    s->size = size;
    s->scope = scope;
    s->isparam = isparam;
    list_pushback(ps->symbols, s);
    printf("symbol %d: %s %d %d %d\n", ps->symbols->count - 1, name, size, scope, isparam);
}

static P_Func* _get_func_byidx(P_State *ps, int fidx) {
    if (fidx >= ps->funcs->count) {
        return NULL;
    }

    lnode *n = ps->funcs->head;
    for (int i = 0; i < fidx; ++i) {
        n = n->next;
    }
    return (P_Func*)n->data;
}

static P_Func* _get_func(P_State *ps, const char *name) {
    for (lnode *n = ps->funcs->head; n != NULL; n = n->next) {
        P_Func *f = (P_Func*)n->data;
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

static void _add_func(P_State *ps, const char *name, int param) {
    if (_get_func(ps, name)) {
        P_FATAL("redefined func");
    }

    P_Func *f = (P_Func*)malloc(sizeof(*f));
    strcpy(f->name, name);
    f->param = param;
    list_pushback(ps->funcs, f);
    printf("func %d: %s %d\n", ps->funcs->count - 1, name, param);
}

P_State* P_newstate(L_State *ls) {
    P_State *ps = (P_State*)malloc(sizeof(*ps));
    ps->ls = ls;
    ps->curfunc = -1;

    ps->symbols = list_new();
    ps->funcs = list_new();
    ps->strs = list_new();
    return ps;
}

void P_freestate(P_State *ps) {
    L_freestate(ps->ls);
    list_free(ps->symbols);
    list_free(ps->funcs);
    list_free(ps->strs);
    free(ps);
}

// parse statements
static void _parse_var(P_State *ps);
static void _parse_func(P_State *ps);

static void _parse_var(P_State *ps) {
    if (L_nexttoken(ps->ls) != L_TT_IDENT) {
        P_FATAL("ident expected by `var'");
    }
    char *id = strdup(ps->ls->curtoken.u.s);

    int size = 1;
    if (L_nexttoken(ps->ls) == L_TT_OPEN_BRACKET) {
        if (L_nexttoken(ps->ls) != L_TT_INT) {
            free(id);
            P_FATAL("int expected by array declare");
        }
        size = ps->ls->curtoken.u.n;
        if (L_nexttoken(ps->ls) != L_TT_CLOSE_BRACKET) {
            free(id);
            P_FATAL("`]' expected by array declare");
        }
    }
    L_cachenexttoken(ps->ls);

    _add_symbol(ps, id, size, ps->curfunc, 0);
    free(id);
}

static void _parse_func(P_State *ps) {
    if (ps->curfunc >= 0) {
        P_FATAL("nested func declare is not allowed");
    }
    ++ps->curfunc;
    if (L_nexttoken(ps->ls) != L_TT_IDENT) {
        P_FATAL("ident expected by `func'");
    }
    char *id = strdup(ps->ls->curtoken.u.s);
    
    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        free(id);
        P_FATAL("`(' expected by func declare");
    }

    int param = 0;
    if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
        L_cachenexttoken(ps->ls);
        for (;;) {
            L_TokenType tt1 = L_nexttoken(ps->ls);
            if (tt1 != L_TT_IDENT) {
                free(id);
                P_FATAL("ident expected by func declare");
            }
            _add_symbol(ps, ps->ls->curtoken.u.s, 1, ps->curfunc, 1);
            ++param;
            if (L_nexttoken(ps->ls) != L_TT_COMMA) {
                L_cachenexttoken(ps->ls);
                break;
            }
        }
        if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
            free(id);
            P_FATAL("`)' expected by func declare");
        }
    }
    _add_func(ps, id, param);
    free(id);

    if (L_nexttoken(ps->ls) != L_TT_OPEN_BRACE) {
        P_FATAL("`{' expected by func declare");
    }
}

void P_add_func_icode(P_State *ps, int fidx, void *icode) {
    P_Func *f = _get_func_byidx(ps, fidx);
    if (f == NULL) {
        P_FATAL("func is NULL");
    }
    list_pushback(f->icodes, icode);
}

void P_parse(P_State *ps) {
    for (;;) {
        L_TokenType tt = L_nexttoken(ps->ls);
        switch (tt) {
            case L_TT_INVALID: {
                P_FATAL("invalid token type");
            } break;
            case L_TT_EOT: {
                if (ps->curfunc >= 0) {
                    P_FATAL("unfinished func declare");
                }
                return;
            } break;

            case L_TT_VAR: {
                _parse_var(ps);
            } break;

            case L_TT_FUNC: {
                _parse_func(ps);
            } break;

            case L_TT_CLOSE_BRACE: {
                if (ps->curfunc < 0) {
                    P_FATAL("unexpect `}'");
                }
                ps->curfunc = -1;
            } break;

            case L_TT_STRING: {
                _add_str(ps, ps->ls->curtoken.u.s);
            }

            default: {
                printf("PARSER: unhandled tokentype: %d\n", tt);
            } break;
        }
    }
}

