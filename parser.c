#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "parser.h"
#include "icode.h"

#define P_FATAL(msg) fatal(__FUNCTION__, __LINE__, msg)

static P_Symbol* _get_symbol(P_State *ps, const char *name);
static void _add_symbol(P_State *ps, const char *name, int size, int scope, int isparam);

static int _get_str(P_State *ps, const char *s);
static void _add_str(P_State *ps, const char *s);

static P_Func* _get_func(P_State *ps, const char *name);
static void _add_func(P_State *ps, const char *name, int param, int ishost);

static int _next_jumpidx(P_State *ps) {
    return ++ps->jumpcount;
}

static int _get_str(P_State *ps, const char *s) {
    int idx = -1;
    for (lnode *n = ps->strs->head; n != NULL; n = n->next) {
        ++idx;
        if (strcmp((char*)n->data, s) == 0) {
            break;
        }
    }
    return idx;
}

static void _add_str(P_State *ps, const char *s) {
    if (_get_str(ps, s) >= 0) {
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
    s->idx = ps->symbols->count;
    strcpy(s->name, name);
    s->size = size;
    s->scope = scope;
    s->isparam = isparam;
    list_pushback(ps->symbols, s);
    printf("symbol %d: %s, size %d, func %d, isparam %d\n", s->idx, name, size, scope, isparam);
}

P_Func* P_get_func_byidx(const P_State *ps, int fidx) {
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

static void _add_func(P_State *ps, const char *name, int param, int ishost) {
    if (_get_func(ps, name)) {
        P_FATAL("redefined func");
    }

    P_Func *f = (P_Func*)malloc(sizeof(*f));
    f->idx = ps->funcs->count;
    strcpy(f->name, name);
    f->param = param;
    f->ishost = ishost;
    f->icodes = list_new();
    list_pushback(ps->funcs, f);

    printf("func %d: %s, param %d, ishost %d\n", f->idx, name, param, ishost);
}

P_State* P_newstate(L_State *ls) {
    P_State *ps = (P_State*)malloc(sizeof(*ps));
    ps->version.major = P_VERSION_MAJOR;
    ps->version.minor = P_VERSION_MINOR;

    ps->ls = ls;
    ps->curfunc = -1;
    ps->jumpcount = 0;

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
static void _parse_statement(P_State *ps);
static void _parse_block(P_State *ps);
static void _parse_func(P_State *ps);
static void _parse_var(P_State *ps);
static void _parse_host(P_State *ps);
static void _parse_exp(P_State *ps);
static void _parse_subexp(P_State *ps);
static void _parse_term(P_State *ps);
static void _parse_factor(P_State *ps);
static void _parse_func_call(P_State *ps, const P_Func *fn);

static void _parse_block(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("code blocks illegal in global scope");
    }
    for (;;) {
        if (L_nexttoken(ps->ls) == L_TT_CLOSE_BRACE) {
            break;
        }
        L_cachenexttoken(ps->ls);
        _parse_statement(ps);
    }
}

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
    } else {
        L_cachenexttoken(ps->ls);
    }
    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by var declare");
    }

    _add_symbol(ps, id, size, ps->curfunc, 0);
    free(id);
}

static void _parse_func(P_State *ps) {
    if (ps->curfunc >= 0) {
        P_FATAL("nested func declare is not allowed");
    }
    ps->curfunc = ps->funcs->count;
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
    _add_func(ps, id, param, 0);
    free(id);

    if (L_nexttoken(ps->ls) != L_TT_OPEN_BRACE) {
        P_FATAL("`{' expected by func declare");
    }
    _parse_block(ps);
    ps->curfunc = -1;
}

static void _parse_host(P_State *ps) {
    if (L_nexttoken(ps->ls) != L_TT_IDENT) {
        P_FATAL("ident expected by host import");
    }
    char *id = strdup(ps->ls->curtoken.u.s);
    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        P_FATAL("`(' expected by host import");
    }
    if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
        P_FATAL("`)' expected by host import");
    }
    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by host import");
    }

    _add_func(ps, id, 0, 1);
    free(id);
}

static void _parse_op_log_and(P_State *ps) {
    int jump_false_idx = _next_jumpidx(ps);
    int jump_exit_idx = _next_jumpidx(ps);

    // POP _T0
    I_Code *POP_1 = I_newinstr(I_OP_POP);
    I_addoperand(POP_1, I_OT_VAR, 0, 0);

    // JE _T0, 0, FALSE
    I_Code *JE_1 = I_newinstr(I_OP_JE);
    I_addoperand(JE_1, I_OT_VAR, 0, 0);
    I_addoperand(JE_1, I_OT_INT, 0, 0);
    I_addoperand(JE_1, I_OT_JUMP, jump_false_idx, 0);

    P_add_func_icode(ps, POP_1);
    P_add_func_icode(ps, JE_1);

    // PUSH op2
    _parse_exp(ps);

    // POP _T0
    I_Code *POP_2 = I_newinstr(I_OP_POP);
    I_addoperand(POP_2, I_OT_VAR, 0, 0);

    // JE _T0, 0, FALSE
    I_Code *JE_2 = I_newinstr(I_OP_JE);
    I_addoperand(JE_2, I_OT_VAR, 0, 0);
    I_addoperand(JE_2, I_OT_INT, 0, 0);
    I_addoperand(JE_2, I_OT_JUMP, jump_false_idx, 0);

    // PUSH 1
    I_Code *PUSH_1 = I_newinstr(I_OP_PUSH);
    I_addoperand(PUSH_1, I_OT_INT, 1, 0);

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, jump_exit_idx, 0);

    // FALSE label
    I_Code *FALSE = I_newjump(jump_false_idx);

    // PUSH 0
    I_Code *PUSH_0 = I_newinstr(I_OP_PUSH);
    I_addoperand(PUSH_0, I_OT_INT, 0, 0);

    // EXIT label
    I_Code *EXIT = I_newjump(jump_exit_idx);

    P_add_func_icode(ps, POP_2);
    P_add_func_icode(ps, JE_2);
    P_add_func_icode(ps, PUSH_1);
    P_add_func_icode(ps, JMP_EXIT);
    P_add_func_icode(ps, FALSE);
    P_add_func_icode(ps, PUSH_0);
    P_add_func_icode(ps, EXIT);
}

static void _parse_op_le(P_State *ps) {
    int jump_true_idx = _next_jumpidx(ps);
    int jump_exit_idx = _next_jumpidx(ps);

    // PUSH op2
    _parse_exp(ps);

    // POP _T1
    I_Code *POP_T1 = I_newinstr(I_OP_POP);
    I_addoperand(POP_T1, I_OT_VAR, 1, 0);

    // POP _T0
    I_Code *POP_T0 = I_newinstr(I_OP_POP);
    I_addoperand(POP_T0, I_OT_VAR, 0, 0);

    // JLE _T0, _T1, TRUE
    I_Code *JLE = I_newinstr(I_OP_JLE);
    I_addoperand(JLE, I_OT_VAR, 0, 0);
    I_addoperand(JLE, I_OT_VAR, 1, 0);
    I_addoperand(JLE, I_OT_JUMP, jump_true_idx, 0);

    // PUSH 0
    I_Code *PUSH_0 = I_newinstr(I_OP_PUSH);
    I_addoperand(PUSH_0, I_OT_INT, 0, 0);

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, jump_exit_idx, 0);

    // TRUE label
    I_Code *TRUE_LABEL = I_newjump(jump_true_idx);

    // PUSH 1
    I_Code *PUSH_1 = I_newinstr(I_OP_PUSH);
    I_addoperand(PUSH_1, I_OT_INT, 1, 0);

    // EXIT label
    I_Code *EXIT_LABEL = I_newjump(jump_exit_idx);

    P_add_func_icode(ps, POP_T1);
    P_add_func_icode(ps, POP_T0);
    P_add_func_icode(ps, JLE);
    P_add_func_icode(ps, PUSH_0);
    P_add_func_icode(ps, JMP_EXIT);
    P_add_func_icode(ps, TRUE_LABEL);
    P_add_func_icode(ps, PUSH_1);
    P_add_func_icode(ps, EXIT_LABEL);
}

static void _parse_op_ass(P_State *ps) {
    mov l, r
}

static void _parse_exp(P_State *ps) {
    _parse_subexp(ps);

    L_TokenType tt = L_nexttoken(ps->ls);
    switch (tt) {
        case L_TT_OP_LOG_AND: {
            _parse_op_log_and(ps);
        } break;

        case L_TT_OP_LE: {
            _parse_op_le(ps);
        } break;

        case L_TT_OP_ASS: {
            _parse_op_ass(ps);
        } break;

        default: {
            L_cachenexttoken(ps->ls);
        } break;
    }
}

static void _parse_subexp(P_State *ps) {
    _parse_term(ps);
    for (;;) {
        L_TokenType tt = L_nexttoken(ps->ls);
        if (tt != L_TT_OP_ADD && tt != L_TT_OP_SUB) {
            L_cachenexttoken(ps->ls);
            break;
        }

        _parse_term(ps);

        I_Code *POP_T1 = I_newinstr(I_OP_POP);
        I_addoperand(POP_T1, I_OT_VAR, 1, 0);

        I_Code *POP_T0 = I_newinstr(I_OP_POP);
        I_addoperand(POP_T0, I_OT_VAR, 0, 0);

        I_Code *ADDSUB_T0_T1 = NULL;
        if (tt == L_TT_OP_ADD) {
            ADDSUB_T0_T1 = I_newinstr(I_OP_ADD);
        } else {
            ADDSUB_T0_T1 = I_newinstr(I_OP_SUB);
        }
        I_addoperand(ADDSUB_T0_T1, I_OT_VAR, 0, 0);
        I_addoperand(ADDSUB_T0_T1, I_OT_VAR, 1, 0);

        I_Code *PUSH_T0 = I_newinstr(I_OP_PUSH);
        I_addoperand(PUSH_T0, I_OT_VAR, 0, 0);

        P_add_func_icode(ps, POP_T1);
        P_add_func_icode(ps, POP_T0);
        P_add_func_icode(ps, ADDSUB_T0_T1);
        P_add_func_icode(ps, PUSH_T0);
    }
}

static void _parse_term(P_State *ps) {
    _parse_factor(ps);
    for (;;) {
        L_TokenType tt = L_nexttoken(ps->ls);
        if (tt != L_TT_OP_MUL && tt != L_TT_OP_DIV) {
            L_cachenexttoken(ps->ls);
            break;
        }

        _parse_factor(ps);

        I_Code *POP_T1 = I_newinstr(I_OP_POP);
        I_addoperand(POP_T1, I_OT_VAR, 1, 0);

        I_Code *POP_T0 = I_newinstr(I_OP_POP);
        I_addoperand(POP_T0, I_OT_VAR, 0, 0);

        I_Code *MULDIV_T0_T1 = NULL;
        if (tt == L_TT_OP_MUL) {
            MULDIV_T0_T1 = I_newinstr(I_OP_MUL);
        } else {
            MULDIV_T0_T1 = I_newinstr(I_OP_DIV);
        }
        I_addoperand(MULDIV_T0_T1, I_OT_VAR, 0, 0);
        I_addoperand(MULDIV_T0_T1, I_OT_VAR, 1, 0);

        I_Code *PUSH_T0 = I_newinstr(I_OP_PUSH);
        I_addoperand(PUSH_T0, I_OT_VAR, 0, 0);

        P_add_func_icode(ps, POP_T1);
        P_add_func_icode(ps, POP_T0);
        P_add_func_icode(ps, MULDIV_T0_T1);
        P_add_func_icode(ps, PUSH_T0);
    }
}

static void _parse_factor(P_State *ps) {
    L_TokenType tt = L_nexttoken(ps->ls);
    if (tt == L_TT_OPEN_PAR) {
        _parse_exp(ps);
        if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
            P_FATAL("`)' expected by exp");
        }
        return;
    }

    switch (tt) {
        case L_TT_INT:
        case L_TT_FLOAT: 
        case L_TT_STRING: 
        case L_TT_TRUE:
        case L_TT_FALSE: 
        case L_TT_IDENT: {
            I_Code *PUSH = I_newinstr(I_OP_PUSH);

            if (tt == L_TT_INT) {
                I_addoperand(PUSH, I_OT_INT, ps->ls->curtoken.u.n, 0);
            } else if (tt == L_TT_FLOAT) {
                I_addoperand(PUSH, I_OT_FLOAT, ps->ls->curtoken.u.f, 0);
            } else if (tt == L_TT_STRING) {
                int idx = _get_str(ps, ps->ls->curtoken.u.s);
                I_addoperand(PUSH, I_OT_STRING, idx, 0);
            } else if (tt == L_TT_TRUE) {
                I_addoperand(PUSH, I_OT_INT, 1, 0);
            } else if (tt == L_TT_FALSE) {
                I_addoperand(PUSH, I_OT_INT, 0, 0);
            } else if (tt == L_TT_IDENT) {
                const char *id = ps->ls->curtoken.u.s;
                P_Symbol *sb = _get_symbol(ps, id);
                if (sb != NULL) {
                    if (sb->scope >= 0 && sb->scope != ps->curfunc) {
                        P_FATAL("var not in current scope");
                    }
                    
                    if (L_nexttoken(ps->ls) == L_TT_OPEN_BRACKET) {
                        if (sb->size == 1) {
                            P_FATAL("unexpected `[' applied to var");
                        }
                        _parse_exp(ps);
                        if (L_nexttoken(ps->ls) != L_TT_CLOSE_BRACKET) {
                            P_FATAL("missing `]'");
                        }
                        I_Code *POP_T0 = I_newinstr(I_OP_POP);
                        I_addoperand(POP_T0, I_OT_VAR, 0, 0);
                        P_add_func_icode(ps, POP_T0);

                        I_addoperand(PUSH, I_OT_ARRAY_REL, sb->idx, 0);
                    } else {
                        L_cachenexttoken(ps->ls);
                        if (sb->size != 1) {
                            P_FATAL("array must be accessed by index");
                        }

                        I_addoperand(PUSH, I_OT_VAR, sb->idx, 0);
                    }
                } else {
                    const P_Func *fn = _get_func(ps, id);
                    if (fn == NULL) {
                        P_FATAL("unexpected ident by exp factor");
                    }
                    _parse_func_call(ps, fn);

                    I_addoperand(PUSH, I_OT_REG, 0, 0);
                }
            }

            P_add_func_icode(ps, PUSH);
        } break;

        case L_TT_OP_ADD:
        case L_TT_OP_SUB: {
            _parse_factor(ps);
            if (tt == L_TT_OP_ADD) {
                return;
            }
            
            I_Code *POP_T0 = I_newinstr(I_OP_POP);
            I_addoperand(POP_T0, I_OT_VAR, 0, 0);

            I_Code *NEG = I_newinstr(I_OP_NEG);
            I_addoperand(NEG, I_OT_VAR, 0, 0);

            I_Code *PUSH_T0 = I_newinstr(I_OP_PUSH);
            I_addoperand(PUSH_T0, I_OT_VAR, 0, 0);

            P_add_func_icode(ps, POP_T0);
            P_add_func_icode(ps, NEG);
            P_add_func_icode(ps, PUSH_T0);
            return;
        }

        default: {
            L_printtoken(&ps->ls->curtoken);
            P_FATAL("unexpected token type by exp factor");
            return;
        }
    }
}

static void _parse_func_call(P_State *ps, const P_Func *fn) {
    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        P_FATAL("`(' expected by function call");
    }

    int param = 0;
    for (;;) {
        if (L_nexttoken(ps->ls) == L_TT_CLOSE_PAR) {
            break;
        }

        _parse_exp(ps);
        if (++param < fn->param) {
            if (L_nexttoken(ps->ls) != L_TT_COMMA) {
                P_FATAL("`,' expected by function call param list");
            }
        }
    }

    if (param != fn->param) {
        P_FATAL("function call param count not match");
    }
    
    I_Code *CALL = I_newinstr(I_OP_CALL);
    I_addoperand(CALL, I_OT_FUNCIDX, fn->idx, 0);

    P_add_func_icode(ps, CALL);
}

void P_add_func_icode(P_State *ps, void *icode) {
    P_Func *f = P_get_func_byidx(ps, ps->curfunc);
    if (f == NULL) {
        P_FATAL("func is NULL");
    }
    list_pushback(f->icodes, icode);
}

void P_parse(P_State *ps) {
    _add_symbol(ps, "_T0", 1, -1, 0);
    _add_symbol(ps, "_T1", 1, -1, 0);

    L_resetstate(ps->ls);
    for (;;) {
        _parse_statement(ps);
        if (L_nexttoken(ps->ls) == L_TT_EOT) {
            break;
        }
        L_cachenexttoken(ps->ls);
    }
}

static void _parse_statement(P_State *ps) {
    L_TokenType tt = L_nexttoken(ps->ls);
    switch (tt) {
        case L_TT_SEM: {} break;
        case L_TT_OPEN_BRACE: {_parse_block(ps);} break;
        case L_TT_FUNC: {_parse_func(ps);} break;
        case L_TT_VAR: {_parse_var(ps);} break;
        case L_TT_HOST: {_parse_host(ps);} break;

        case L_TT_INT:
        case L_TT_FLOAT:
        case L_TT_OP_ADD:
        case L_TT_OP_SUB:
        case L_TT_IDENT:
        case L_TT_OPEN_PAR: {
            L_cachenexttoken(ps->ls);
            _parse_exp(ps);
        } break;

        case L_TT_EOT: {
            P_FATAL("unexpected end of file");
        } break;
        default: {
            L_printtoken(&ps->ls->curtoken);
            P_FATAL("unexpected token");
        } break;
    }
}
