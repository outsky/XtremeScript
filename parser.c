#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "parser.h"
#include "icode.h"

//#define P_DEBUG

#define P_FATAL(msg) fatal(__FILE__, __LINE__, msg)

#define _POP(t, n) do {\
    I_Code *c = I_newinstr(I_OP_POP);\
    I_addoperand(c, t, n, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define POP_T0 _POP(I_OT_VAR, 0)
#define POP_T1 _POP(I_OT_VAR, 1)
#define POP_RetVal _POP(I_OT_REG, 0)

#define _PUSH(t, n) do {\
    I_Code *c = I_newinstr(I_OP_PUSH);\
    I_addoperand(c, t, n, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define PUSH_0 _PUSH(I_OT_INT, 0)
#define PUSH_1 _PUSH(I_OT_INT, 1)
#define PUSH_T0 _PUSH(I_OT_VAR, 0)
#define PUSH_T1 _PUSH(I_OT_VAR, 1)

#define GetJumpLabels() \
    int label1 = _next_jumpidx(ps);\
    int label2 = _next_jumpidx(ps)

#define _LABEL(l) do {\
    I_Code *c = I_newjump(l);\
    P_add_func_icode(ps, c);\
} while (0)

#define LABEL_1 _LABEL(label1)
#define LABEL_2 _LABEL(label2)

static P_Symbol* _get_symbol(P_State *ps, const char *name, int scope);
static void _add_symbol(P_State *ps, const char *name, int size, int scope, int isparam);

static int _get_str(P_State *ps, const char *s);
static int _add_str(P_State *ps, const char *s);

static P_Func* _get_func(P_State *ps, const char *name);
static void _add_func(P_State *ps, const char *name, int param, int ishost);

static int _next_jumpidx(P_State *ps) {
    return ++ps->jumpcount;
}

static int _get_break_label(P_State *ps) {
    int label = ps->breaklabel;
    ps->breaklabel = -1;
    return label;
}
static void _set_break_label(P_State *ps, int label) {
    ps->breaklabel = label;
}

static int _get_continue_label(P_State *ps) {
    int label = ps->continuelabel;
    ps->continuelabel = -1;
    return label;
}
static void _set_continue_label(P_State *ps, int label) {
    ps->continuelabel = label;
}

static int _get_str(P_State *ps, const char *s) {
    int idx = 0;
    for (lnode *n = ps->strs->head; n != NULL; n = n->next) {
        if (strcmp((char*)n->data, s) == 0) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

static int _add_str(P_State *ps, const char *s) {
    int idx = _get_str(ps, s);
    if (idx >= 0) {
        return idx;
    }
    list_pushback(ps->strs, strdup(s));
    return ps->strs->count - 1;
}

static P_Symbol* _get_symbol(P_State *ps, const char *name, int scope) {
    P_Symbol *gs = NULL;
    for (lnode *n = ps->symbols->head; n != NULL; n = n->next) {
        P_Symbol *s = (P_Symbol*)n->data;
        if (strcmp(s->name, name) == 0) {
            if (s->scope == -1) {
                gs = s;
            } else if (s->scope == scope) {
                return s;
            }
        }
    }
    return gs;
}

static void _add_symbol(P_State *ps, const char *name, int size, int scope, int isparam) {
    P_Symbol *s = _get_symbol(ps, name, scope);
    if (s != NULL && s->scope == scope) {
        P_FATAL("redefined symbol");
    }

    s = (P_Symbol*)malloc(sizeof(*s));
    s->idx = ps->symbols->count;
    strcpy(s->name, name);
    s->size = size;
    s->scope = scope;
    s->isparam = isparam;
    list_pushback(ps->symbols, s);
#ifdef P_DEBUG
    printf("symbol %d: %s, size %d, func %d, isparam %d\n", s->idx, name, size, scope, isparam);
#endif
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

#ifdef P_DEBUG
    printf("func %d: %s, param %d, ishost %d\n", f->idx, name, param, ishost);
#endif
}

P_State* P_newstate(L_State *ls) {
    P_State *ps = (P_State*)malloc(sizeof(*ps));
    ps->version.major = P_VERSION_MAJOR;
    ps->version.minor = P_VERSION_MINOR;

    ps->ls = ls;
    ps->curfunc = -1;
    ps->jumpcount = 0;

    ps->breaklabel = -1;
    ps->continuelabel = -1;

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
static void _parse_func_call(P_State *ps);
static void _parse_assign(P_State *ps);
static void _parse_return(P_State *ps);
static void _parse_while(P_State *ps);
static void _parse_break(P_State *ps);
static void _parse_continue(P_State *ps);
static void _parse_if(P_State *ps);

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
    GetJumpLabels();

    POP_T0;

    // JE _T0, 0, FALSE
    I_Code *JE_1 = I_newinstr(I_OP_JE);
    I_addoperand(JE_1, I_OT_VAR, 0, 0);
    I_addoperand(JE_1, I_OT_INT, 0, 0);
    I_addoperand(JE_1, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JE_1);

    // do exp
    _parse_exp(ps);

    POP_T0;

    // JE _T0, 0, FALSE
    I_Code *JE_2 = I_newinstr(I_OP_JE);
    I_addoperand(JE_2, I_OT_VAR, 0, 0);
    I_addoperand(JE_2, I_OT_INT, 0, 0);
    I_addoperand(JE_2, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JE_2);

    PUSH_1;

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_EXIT);

    LABEL_1;
    PUSH_0;
    LABEL_2;
}

static void _parse_op_log_or(P_State *ps) {
    GetJumpLabels();

    POP_T0;

    // JNE _T0, 0, TRUE 
    I_Code *JNE_1 = I_newinstr(I_OP_JNE);
    I_addoperand(JNE_1, I_OT_VAR, 0, 0);
    I_addoperand(JNE_1, I_OT_INT, 0, 0);
    I_addoperand(JNE_1, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JNE_1);

    // PUSH op2
    _parse_exp(ps);

    POP_T0;

    // JNE _T0, 0, TRUE
    I_Code *JNE_2 = I_newinstr(I_OP_JNE);
    I_addoperand(JNE_2, I_OT_VAR, 0, 0);
    I_addoperand(JNE_2, I_OT_INT, 0, 0);
    I_addoperand(JNE_2, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JNE_2);

    PUSH_0;

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_EXIT);

    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_log_not(P_State *ps) {
    GetJumpLabels();

    // do exp
    _parse_exp(ps);

    POP_T0;

    // JE _T0, 0, TRUE
    I_Code *JE = I_newinstr(I_OP_JE);
    I_addoperand(JE, I_OT_VAR, 0, 0);
    I_addoperand(JE, I_OT_INT, 0, 0);
    I_addoperand(JE, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JE);

    PUSH_0;

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_EXIT);
   
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_log_eq(P_State *ps) {
    GetJumpLabels();

    POP_T0;

    // do exp
    _parse_exp(ps);

    POP_T1;

    // JE _T0, _T1, TRUE
    I_Code *JE = I_newinstr(I_OP_JE);
    I_addoperand(JE, I_OT_VAR, 0, 0);
    I_addoperand(JE, I_OT_VAR, 1, 0);
    I_addoperand(JE, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JE);

    PUSH_0;

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_EXIT);
    
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_relational(P_State *ps, I_OpCode op) {
    GetJumpLabels();

    // PUSH op2
    _parse_exp(ps);

    POP_T1;
    POP_T0;

    // <OP> _T0, _T1, TRUE
    I_Code *OP = I_newinstr(op);
    I_addoperand(OP, I_OT_VAR, 0, 0);
    I_addoperand(OP, I_OT_VAR, 1, 0);
    I_addoperand(OP, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, OP);

    PUSH_0;

    // JMP EXIT
    I_Code *JMP_EXIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_EXIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_EXIT);

    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_bitwise(P_State *ps, I_OpCode op) {
    POP_T0;

    // do right
    _parse_exp(ps);

    POP_T1;

    // <OP> _T0, _T1
    I_Code *OP = I_newinstr(op);
    I_addoperand(OP, I_OT_VAR, 0, 0);
    I_addoperand(OP, I_OT_VAR, 1, 0);
    P_add_func_icode(ps, OP);

    PUSH_T0;
}

static void _parse_op_exp(P_State *ps) {
    POP_T0;

    // do right
    _parse_exp(ps);

    POP_T1;

    // EXP _T0, _T1
    I_Code *OP = I_newinstr(I_OP_EXP);
    I_addoperand(OP, I_OT_VAR, 0, 0);
    I_addoperand(OP, I_OT_VAR, 1, 0);
    P_add_func_icode(ps, OP);

    PUSH_T0;
}

static void _parse_exp(P_State *ps) {
    _parse_subexp(ps);

    L_TokenType tt = L_nexttoken(ps->ls);
    switch (tt) {
        case L_TT_OP_LOG_AND: {_parse_op_log_and(ps);} break;
        case L_TT_OP_LOG_OR: {_parse_op_log_or(ps);} break;
        case L_TT_OP_LOG_NOT: {_parse_op_log_not(ps);} break;
        case L_TT_OP_LOG_EQ: {_parse_op_log_eq(ps);} break;
        
        case L_TT_OP_BIT_AND: {_parse_op_bitwise(ps, I_OP_AND);} break;
        case L_TT_OP_BIT_OR: {_parse_op_bitwise(ps, I_OP_OR);} break;

        case L_TT_OP_L: {_parse_op_relational(ps, I_OP_JL);} break;
        case L_TT_OP_G: {_parse_op_relational(ps, I_OP_JG);} break;
        case L_TT_OP_LE: {_parse_op_relational(ps, I_OP_JLE);} break;
        case L_TT_OP_GE: {_parse_op_relational(ps, I_OP_JGE);} break;

        case L_TT_OP_EXP: {_parse_op_exp(ps);} break;

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

        POP_T1;
        POP_T0;

        // ADD|SUB _T0, _T1
        I_Code *ADDSUB_T0_T1 = NULL;
        if (tt == L_TT_OP_ADD) {
            ADDSUB_T0_T1 = I_newinstr(I_OP_ADD);
        } else {
            ADDSUB_T0_T1 = I_newinstr(I_OP_SUB);
        }
        I_addoperand(ADDSUB_T0_T1, I_OT_VAR, 0, 0);
        I_addoperand(ADDSUB_T0_T1, I_OT_VAR, 1, 0);
        P_add_func_icode(ps, ADDSUB_T0_T1);

        PUSH_T0;
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

        POP_T1;
        POP_T0;

        // MUL|DIV _T0, _T1
        I_Code *MULDIV_T0_T1 = NULL;
        if (tt == L_TT_OP_MUL) {
            MULDIV_T0_T1 = I_newinstr(I_OP_MUL);
        } else {
            MULDIV_T0_T1 = I_newinstr(I_OP_DIV);
        }
        I_addoperand(MULDIV_T0_T1, I_OT_VAR, 0, 0);
        I_addoperand(MULDIV_T0_T1, I_OT_VAR, 1, 0);
        P_add_func_icode(ps, MULDIV_T0_T1);

        PUSH_T0;
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
                int idx = _add_str(ps, ps->ls->curtoken.u.s);
                I_addoperand(PUSH, I_OT_STRING, idx, 0);
            } else if (tt == L_TT_TRUE) {
                I_addoperand(PUSH, I_OT_INT, 1, 0);
            } else if (tt == L_TT_FALSE) {
                I_addoperand(PUSH, I_OT_INT, 0, 0);
            } else if (tt == L_TT_IDENT) {
                const char *id = ps->ls->curtoken.u.s;
                P_Symbol *sb = _get_symbol(ps, id, ps->curfunc);
                if (sb != NULL) {
                    if (L_nexttoken(ps->ls) == L_TT_OPEN_BRACKET) {
                        if (sb->size == 1) {
                            P_FATAL("unexpected `[' applied to var");
                        }
                        _parse_exp(ps);
                        if (L_nexttoken(ps->ls) != L_TT_CLOSE_BRACKET) {
                            P_FATAL("missing `]'");
                        }
                        POP_T0;

                        I_addoperand(PUSH, I_OT_ARRAY_REL, sb->idx, 0);
                    } else {
                        L_cachenexttoken(ps->ls);
                        if (sb->size != 1) {
                            P_FATAL("array must be accessed by index");
                        }

                        I_addoperand(PUSH, I_OT_VAR, sb->idx, 0);
                    }
                } else {
                    _parse_func_call(ps);
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
            
            POP_T0;

            I_Code *NEG = I_newinstr(I_OP_NEG);
            I_addoperand(NEG, I_OT_VAR, 0, 0);
            P_add_func_icode(ps, NEG);

            PUSH_T0;
            return;
        }

        default: {
            L_printtoken(&ps->ls->curtoken);
            P_FATAL("unexpected token type by exp factor");
            return;
        }
    }
}

static void _parse_func_call(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("function call cant in global scope");
    }

    const P_Func *fn = _get_func(ps, ps->ls->curtoken.u.s);
    if (fn == NULL) {
        P_FATAL("not a function");
    }
    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        P_FATAL("`(' expected by function call");
    }

    int param = 0;
    for (;;) {
        if (L_nexttoken(ps->ls) == L_TT_CLOSE_PAR) {
            break;
        }
        L_cachenexttoken(ps->ls);

        _parse_exp(ps);

        if (++param < fn->param) {
            if (L_nexttoken(ps->ls) != L_TT_COMMA) {
                P_FATAL("`,' expected by function call param list");
            }
        }
    }

    if (!fn->ishost && param != fn->param) {
        P_FATAL("function call param count not match");
    }
    
    // CALL|CALLHOST func
    I_Code *CALL = NULL;
    if (fn->ishost) {
        CALL = I_newinstr(I_OP_CALLHOST);
    } else {
        CALL = I_newinstr(I_OP_CALL);
    }
    I_addoperand(CALL, I_OT_FUNCIDX, fn->idx, 0);
    P_add_func_icode(ps, CALL);
}

static void _parse_assign(P_State *ps) {
    const char *id = ps->ls->curtoken.u.s;
    const P_Symbol *sb = _get_symbol(ps, id, ps->curfunc);
    if (sb == NULL) {
        P_FATAL("not a var");
    }
    if (sb->size > 1) {
        if (L_nexttoken(ps->ls) != L_TT_OPEN_BRACKET) {
            P_FATAL("`[' expected by array access");
        }
        _parse_exp(ps);
        if (L_nexttoken(ps->ls) != L_TT_CLOSE_BRACKET) {
            P_FATAL("`]' expected by array access");
        }
    }

    I_OpCode opc = -1;
    L_TokenType tt = L_nexttoken(ps->ls);
    switch (tt) {
        case L_TT_OP_ASS: {opc = I_OP_MOV;} break;
        case L_TT_OP_ADDASS: {opc = I_OP_ADD;} break;
        case L_TT_OP_SUBASS: {opc = I_OP_SUB;} break;
        case L_TT_OP_MULASS: {opc = I_OP_MUL;} break;
        case L_TT_OP_DIVASS: {opc = I_OP_DIV;} break;
        case L_TT_OP_MODASS: {opc = I_OP_MOD;} break;
        case L_TT_OP_EXPASS: {opc = I_OP_EXP;} break;
        case L_TT_OP_BIT_ANDASS: {opc = I_OP_AND;} break;
        case L_TT_OP_BIT_ORASS: {opc = I_OP_OR;} break;
        case L_TT_OP_BIT_XORASS: {opc = I_OP_XOR;} break;
        case L_TT_OP_BIT_SLEFTASS: {opc = I_OP_SHL;} break;
        case L_TT_OP_BIT_SRIGHTASS: {opc = I_OP_SHR;} break;

        default: {
            P_FATAL("assign operator expected");
        }
    }
    I_Code *OP = I_newinstr(opc);

    _parse_exp(ps);
    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by assign");
    }

    if (sb->size == 1) {
        /* var */
        POP_T0;

        // <OP> v, _T0
        I_addoperand(OP, I_OT_VAR, sb->idx, 0);
        I_addoperand(OP, I_OT_VAR, 0, 0);
        P_add_func_icode(ps, OP);
    } else {
        /* array */
        POP_T1;
        POP_T0;

        // <OP> v[_T0], _T1
        I_addoperand(OP, I_OT_ARRAY_REL, sb->idx, 0);
        I_addoperand(OP, I_OT_VAR, 1, 0);
        P_add_func_icode(ps, OP);
    }
}

static void _parse_return(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("return cant in global scope");
    }

    if (L_nexttoken(ps->ls) == L_TT_SEM) {
        return;
    }
    L_cachenexttoken(ps->ls);

    _parse_exp(ps);

    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by return");
    }

    POP_RetVal;
}

static void _parse_while(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("while cant in global scope");
    }

    GetJumpLabels();
    _set_continue_label(ps, label1);
    _set_break_label(ps, label2);

    LABEL_1;

    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        P_FATAL("`(' expected by while");
    }
    // do condition
    _parse_exp(ps);
    if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
        P_FATAL("`)' expected by while");
    }

    POP_T0;

    // JE _T0, 0, QUIT
    I_Code *JE = I_newinstr(I_OP_JE);
    I_addoperand(JE, I_OT_VAR, 0, 0);
    I_addoperand(JE, I_OT_INT, 0, 0);
    I_addoperand(JE, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JE);

    // do body
    _parse_statement(ps);

    // JMP LOOP
    I_Code *JMP_LOOP = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_LOOP, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JMP_LOOP);

    LABEL_2;
}

static void _parse_break(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("break cant in global scope");
    }

    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by break");
    }

    int label = _get_break_label(ps);
    if (label < 0) {
        P_FATAL("break is not allowed here");
    }

    // JMP label
    I_Code *JMP = I_newinstr(I_OP_JMP);
    I_addoperand(JMP, I_OT_JUMP, label, 0);
    P_add_func_icode(ps, JMP);
}

static void _parse_continue(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("continue cant in global scope");
    }

    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        P_FATAL("`;' expected by continue");
    }

    int label = _get_continue_label(ps);
    if (label < 0) {
        P_FATAL("continue is not allowed here");
    }

    // JMP label
    I_Code *JMP = I_newinstr(I_OP_JMP);
    I_addoperand(JMP, I_OT_JUMP, label, 0);
    P_add_func_icode(ps, JMP);
}

static void _parse_if(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("if cant in global scope");
    }

    if (L_nexttoken(ps->ls) != L_TT_OPEN_PAR) {
        P_FATAL("`(' expected by if");
    }

    // do condition
    _parse_exp(ps);

    POP_T0;

    GetJumpLabels();

    // JE _T0, 0, ELSE
    I_Code *JE = I_newinstr(I_OP_JE);
    I_addoperand(JE, I_OT_VAR, 0, 0);
    I_addoperand(JE, I_OT_INT, 0, 0);
    I_addoperand(JE, I_OT_JUMP, label1, 0);
    P_add_func_icode(ps, JE);

    if (L_nexttoken(ps->ls) != L_TT_CLOSE_PAR) {
        P_FATAL("`)' expected by if");
    }

    // do true block
    _parse_statement(ps);

    // JMP QUIT
    I_Code *JMP_QUIT = I_newinstr(I_OP_JMP);
    I_addoperand(JMP_QUIT, I_OT_JUMP, label2, 0);
    P_add_func_icode(ps, JMP_QUIT);

    LABEL_1;

    if (L_nexttoken(ps->ls) == L_TT_ELSE) {
        // do false block
        _parse_statement(ps);
    } else {
        L_cachenexttoken(ps->ls);
    }
    
    LABEL_2;
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

        case L_TT_IDENT: {
            const char *id = ps->ls->curtoken.u.s;
            if (_get_symbol(ps, id, ps->curfunc) != NULL) {
                _parse_assign(ps);
                break;
            }
            _parse_func_call(ps);
            if (L_nexttoken(ps->ls) != L_TT_SEM) {
                P_FATAL("`;' expected by function call");
            }
        } break;

        case L_TT_RETURN: {_parse_return(ps);} break;
        case L_TT_WHILE: {_parse_while(ps);} break;
        case L_TT_BREAK: {_parse_break(ps);} break;
        case L_TT_CONTINUE: {_parse_continue(ps);} break;
        case L_TT_IF: {_parse_if(ps);} break;

        default: {
            L_printtoken(&ps->ls->curtoken);
            P_FATAL("unexpected token");
        } break;
    }
}
