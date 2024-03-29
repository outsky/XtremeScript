#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "parser.h"
#include "icode.h"

//#define P_DEBUG

#define P_FATAL(...) codesnap(); error(__VA_ARGS__)
#define codesnap() snapshot(ps->ls->source, ps->ls->curidx, ps->ls->curline)
#define expect(tt) do {\
    if (L_nexttoken(ps->ls) != tt) {\
        codesnap();\
        snapshot(ps->ls->source, ps->ls->curidx, ps->ls->curline);\
        error("`%s' expected, but got `%s'", L_ttname(tt), L_ttname(ps->ls->curtoken.type));\
    }\
} while (0)

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

#define getjmplabels() \
    int label1 = _next_jumpidx(ps);\
    int label2 = _next_jumpidx(ps)

#define _LABEL(l) do {\
    I_Code *c = I_newjump(l);\
    P_add_func_icode(ps, c);\
} while (0)

#define LABEL_1 _LABEL(label1)
#define LABEL_2 _LABEL(label2)

#define _JMP(l) do {\
    I_Code *c = I_newinstr(I_OP_JMP);\
    I_addoperand(c, I_OT_JUMP, l, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define JMP_LABEL_1 _JMP(label1)
#define JMP_LABEL_2 _JMP(label2)

#define _COND_JMP(op, ot1, n1, ot2, n2, label) do {\
    I_Code *c = I_newinstr(op);\
    I_addoperand(c, ot1, n1, 0);\
    I_addoperand(c, ot2, n2, 0);\
    I_addoperand(c, I_OT_JUMP, label, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define JE_T0_0_LABEL_1 _COND_JMP(I_OP_JE, I_OT_VAR, 0, I_OT_INT, 0, label1)
#define JE_T0_0_LABEL_2 _COND_JMP(I_OP_JE, I_OT_VAR, 0, I_OT_INT, 0, label2)
#define JNE_T0_0_LABEL_1 _COND_JMP(I_OP_JNE, I_OT_VAR, 0, I_OT_INT, 0, label1)
#define JNE_T0_0_LABEL_2 _COND_JMP(I_OP_JNE, I_OT_VAR, 0, I_OT_INT, 0, label2)
#define JE_T0_T1_LABEL_1 _COND_JMP(I_OP_JE, I_OT_VAR, 0, I_OT_VAR, 1, label1)
#define JE_T0_T1_LABEL_2 _COND_JMP(I_OP_JE, I_OT_VAR, 0, I_OT_VAR, 1, label2)
#define JNE_T0_T1_LABEL_1 _COND_JMP(I_OP_JNE, I_OT_VAR, 0, I_OT_VAR, 1, label1)
#define JNE_T0_T1_LABEL_2 _COND_JMP(I_OP_JNE, I_OT_VAR, 0, I_OT_VAR, 1, label2)

#define _OP_T0(op) do {\
    I_Code *c = I_newinstr(op);\
    I_addoperand(c, I_OT_VAR, 0, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define _OP_T0_T1(op) do {\
    I_Code *c = I_newinstr(op);\
    I_addoperand(c, I_OT_VAR, 0, 0);\
    I_addoperand(c, I_OT_VAR, 1, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define _OP_T0_T1_JMP(op, label) do {\
    I_Code *c = I_newinstr(op);\
    I_addoperand(c, I_OT_VAR, 0, 0);\
    I_addoperand(c, I_OT_VAR, 1, 0);\
    I_addoperand(c, I_OT_JUMP, label, 0);\
    P_add_func_icode(ps, c);\
} while (0)

#define NEG_T0 _OP_T0(I_OP_NEG)
#define INC_T0 _OP_T0(I_OP_INC)
#define DEC_T0 _OP_T0(I_OP_DEC)
#define EXP_T0_T1 _OP_T0_T1(I_OP_EXP)
#define ADD_T0_T1 _OP_T0_T1(I_OP_ADD)
#define SUB_T0_T1 _OP_T0_T1(I_OP_SUB)
#define MUL_T0_T1 _OP_T0_T1(I_OP_MUL)
#define DIV_T0_T1 _OP_T0_T1(I_OP_DIV)
#define MOD_T0_T1 _OP_T0_T1(I_OP_MOD)

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
static void _parse_for(P_State *ps);
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
    expect(L_TT_IDENT);
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
    expect(L_TT_SEM);

    _add_symbol(ps, id, size, ps->curfunc, 0);
    free(id);
}

static void _parse_func(P_State *ps) {
    if (ps->curfunc >= 0) {
        P_FATAL("nested func declare is not allowed");
    }
    ps->curfunc = ps->funcs->count;
    expect(L_TT_IDENT);
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

    expect(L_TT_OPEN_BRACE);
    _parse_block(ps);
    ps->curfunc = -1;
}

static void _parse_host(P_State *ps) {
    expect(L_TT_IDENT);
    char *id = strdup(ps->ls->curtoken.u.s);
    expect(L_TT_OPEN_PAR);
    expect(L_TT_CLOSE_PAR);
    expect(L_TT_SEM);

    _add_func(ps, id, 0, 1);
    free(id);
}

static void _parse_op_log_and(P_State *ps) {
    getjmplabels();

    POP_T0;
    JE_T0_0_LABEL_1;

    _parse_exp(ps);

    POP_T0;
    JE_T0_0_LABEL_1;
    PUSH_1;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_0;
    LABEL_2;
}

static void _parse_op_log_or(P_State *ps) {
    getjmplabels();

    POP_T0;
    JNE_T0_0_LABEL_1;

    _parse_exp(ps);

    POP_T0;
    JNE_T0_0_LABEL_1;
    PUSH_0;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_log_not(P_State *ps) {
    getjmplabels();

    _parse_exp(ps);

    POP_T0;
    JE_T0_0_LABEL_1;
    PUSH_0;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_log_eq(P_State *ps) {
    getjmplabels();

    _parse_exp(ps);

    POP_T1;
    POP_T0;
    JE_T0_T1_LABEL_1;
    PUSH_0;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_log_neq(P_State *ps) {
    getjmplabels();

    POP_T0;

    _parse_exp(ps);

    POP_T1;
    JNE_T0_T1_LABEL_1;
    PUSH_0;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_relational(P_State *ps, I_OpCode op) {
    getjmplabels();

    _parse_exp(ps);

    POP_T1;
    POP_T0;
    _OP_T0_T1_JMP(op, label1);
    PUSH_0;
    JMP_LABEL_2;
    LABEL_1;
    PUSH_1;
    LABEL_2;
}

static void _parse_op_bitwise(P_State *ps, I_OpCode op) {
    POP_T0;

    _parse_exp(ps);

    POP_T1;
    _OP_T0_T1(op);
    PUSH_T0;
}

static void _parse_op_exp(P_State *ps) {
    POP_T0;

    _parse_exp(ps);

    POP_T1;
    EXP_T0_T1;
    PUSH_T0;
}

static void _parse_op_inc_pre(P_State *ps) {
    _parse_exp(ps);
    POP_T0;
    INC_T0;
    PUSH_T0;
}

static void _parse_op_inc_post(P_State *ps) {
    POP_T0;
    INC_T0;
    PUSH_T0;
}

static void _parse_op_dec_pre(P_State *ps) {
    _parse_exp(ps);
    POP_T0;
    DEC_T0;
    PUSH_T0;
}

static void _parse_op_dec_post(P_State *ps) {
    POP_T0;
    DEC_T0;
    PUSH_T0;
}

static void _parse_exp(P_State *ps) {
    _parse_subexp(ps);

    L_TokenType tt = L_nexttoken(ps->ls);
    switch (tt) {
        case L_TT_OP_EXP: {_parse_op_exp(ps);} break;
        case L_TT_OP_INC: {_parse_op_inc_post(ps);} break;
        case L_TT_OP_DEC: {_parse_op_dec_post(ps);} break;

        case L_TT_OP_LOG_AND: {_parse_op_log_and(ps);} break;
        case L_TT_OP_LOG_OR: {_parse_op_log_or(ps);} break;
        case L_TT_OP_LOG_EQ: {_parse_op_log_eq(ps);} break;
        case L_TT_OP_LOG_NEQ: {_parse_op_log_neq(ps);} break;
        
        case L_TT_OP_BIT_AND: {_parse_op_bitwise(ps, I_OP_AND);} break;
        case L_TT_OP_BIT_OR: {_parse_op_bitwise(ps, I_OP_OR);} break;

        case L_TT_OP_L: {_parse_op_relational(ps, I_OP_JL);} break;
        case L_TT_OP_G: {_parse_op_relational(ps, I_OP_JG);} break;
        case L_TT_OP_LE: {_parse_op_relational(ps, I_OP_JLE);} break;
        case L_TT_OP_GE: {_parse_op_relational(ps, I_OP_JGE);} break;

        default: {
            L_cachenexttoken(ps->ls);
        } break;
    }
}

static void _parse_subexp(P_State *ps) {
    _parse_term(ps);
    for (;;) {
        L_TokenType tt = L_nexttoken(ps->ls);
        if (tt != L_TT_OP_ADD && tt != L_TT_OP_SUB && tt != L_TT_OP_MOD) {
            L_cachenexttoken(ps->ls);
            break;
        }

        _parse_term(ps);

        POP_T1;
        POP_T0;

        if (tt == L_TT_OP_ADD) {
            ADD_T0_T1;
        } else if (tt == L_TT_OP_SUB) {
            SUB_T0_T1;
        } else if (tt == L_TT_OP_MOD) {
            MOD_T0_T1;
        }

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

        if (tt == L_TT_OP_MUL) {
            MUL_T0_T1;
        } else {
            DIV_T0_T1;
        }

        PUSH_T0;
    }
}

static void _parse_factor(P_State *ps) {
    L_TokenType tt = L_nexttoken(ps->ls);
    if (tt == L_TT_OPEN_PAR) {
        _parse_exp(ps);
        expect(L_TT_CLOSE_PAR);
        return;
    }

    switch (tt) {
        case L_TT_INT:
        case L_TT_FLOAT: 
        case L_TT_STRING: 
        case L_TT_TRUE:
        case L_TT_FALSE: 
        case L_TT_IDENT: {
            if (tt == L_TT_INT) {
                _PUSH(I_OT_INT, ps->ls->curtoken.u.n);
            } else if (tt == L_TT_FLOAT) {
                _PUSH(I_OT_FLOAT, ps->ls->curtoken.u.f);
            } else if (tt == L_TT_STRING) {
                int idx = _add_str(ps, ps->ls->curtoken.u.s);
                _PUSH(I_OT_STRING, idx);
            } else if (tt == L_TT_TRUE) {
                _PUSH(I_OT_INT, 1);
            } else if (tt == L_TT_FALSE) {
                _PUSH(I_OT_INT, 0);
            } else if (tt == L_TT_IDENT) {
                const char *id = ps->ls->curtoken.u.s;
                P_Symbol *sb = _get_symbol(ps, id, ps->curfunc);
                if (sb != NULL) {
                    if (L_nexttoken(ps->ls) == L_TT_OPEN_BRACKET) {
                        if (sb->size == 1) {
                            P_FATAL("unexpected `[' applied to var");
                        }
                        _parse_exp(ps);
                        expect(L_TT_CLOSE_BRACKET);
                        POP_T0;

                        _PUSH(I_OT_ARRAY_REL, sb->idx);
                    } else {
                        L_cachenexttoken(ps->ls);
                        if (sb->size != 1) {
                            P_FATAL("array must be accessed by index");
                        }

                        _PUSH(I_OT_VAR, sb->idx);
                    }
                } else {
                    _parse_func_call(ps);
                    _PUSH(I_OT_REG, 0);
                }
            }
        } break;

        case L_TT_OP_ADD:
        case L_TT_OP_SUB: {
            _parse_factor(ps);
            if (tt == L_TT_OP_ADD) {
                break;
            }
            
            POP_T0;
            NEG_T0;
            PUSH_T0;
        } break;

        case L_TT_OP_INC: {_parse_op_inc_pre(ps);} break;
        case L_TT_OP_DEC: {_parse_op_dec_pre(ps);} break;
        case L_TT_OP_LOG_NOT: {_parse_op_log_not(ps);} break;

        default: {
            P_FATAL("%s: unexpected `%s'", __FUNCTION__, L_ttname(tt));
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
        P_FATAL("`%s' is not a function", ps->ls->curtoken.u.s);
    }
    expect(L_TT_OPEN_PAR);

    int param = 0;
    for (;;) {
        if (L_nexttoken(ps->ls) == L_TT_CLOSE_PAR) {
            break;
        }
        L_cachenexttoken(ps->ls);

        _parse_exp(ps);

        if (++param < fn->param) {
            expect(L_TT_COMMA);
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
        expect(L_TT_OPEN_BRACKET);
        _parse_exp(ps);
        expect(L_TT_CLOSE_BRACKET);
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
        case L_TT_OP_INC: {_parse_op_inc_post(ps);} return;

        default: {
            codesnap();
            P_FATAL("%s: unexpected `%s'", __FUNCTION__, L_ttname(tt));
        }
    }
    I_Code *OP = I_newinstr(opc);

    _parse_exp(ps);

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

    expect(L_TT_SEM);

    POP_RetVal;
}

static void _parse_while(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("while cant in global scope");
    }

    getjmplabels();
    _set_continue_label(ps, label1);
    _set_break_label(ps, label2);

    LABEL_1;

    expect(L_TT_OPEN_PAR);
    _parse_exp(ps);
    expect(L_TT_CLOSE_PAR);

    POP_T0;
    JE_T0_0_LABEL_2;

    _parse_statement(ps);

    JMP_LABEL_1;
    LABEL_2;
}

static void _parse_for(P_State *ps) {
    getjmplabels();
    _set_break_label(ps, label2);

    expect(L_TT_OPEN_PAR);
    // do init
    if (L_nexttoken(ps->ls) == L_TT_IDENT) {
        _parse_assign(ps);
    } else {
        L_cachenexttoken(ps->ls);
    }
    expect(L_TT_SEM);

    LABEL_1;

    // do condition
    if (L_nexttoken(ps->ls) != L_TT_SEM) {
        L_cachenexttoken(ps->ls);
        _parse_exp(ps);
        expect(L_TT_SEM);
    } else {
        PUSH_1;
    }

    POP_T0;
    JE_T0_0_LABEL_2;

    int label3 = _next_jumpidx(ps);
    int label4 = _next_jumpidx(ps);
    _set_continue_label(ps, label3);

    _JMP(label4);
    _LABEL(label3);
    // do after
    if (L_nexttoken(ps->ls) == L_TT_IDENT) {
        _parse_assign(ps);
    } else {
        L_cachenexttoken(ps->ls);
    }
    expect(L_TT_CLOSE_PAR);
    JMP_LABEL_1;

    _LABEL(label4);
    // do body
    _parse_statement(ps);
    _JMP(label3);

    LABEL_2;
}

static void _parse_break(P_State *ps) {
    expect(L_TT_SEM);

    int label = _get_break_label(ps);
    if (label < 0) {
        P_FATAL("break statement not within loop");
    }
    _JMP(label);
}

static void _parse_continue(P_State *ps) {
    if (ps->curfunc < 0) {
        P_FATAL("continue cant in global scope");
    }

    expect(L_TT_SEM);

    int label = _get_continue_label(ps);
    if (label < 0) {
        P_FATAL("continue is not allowed here");
    }
    _JMP(label);
}

static void _parse_if(P_State *ps) {
    getjmplabels();

    if (ps->curfunc < 0) {
        P_FATAL("if cant in global scope");
    }

    expect(L_TT_OPEN_PAR);

    // do condition
    _parse_exp(ps);

    POP_T0;
    JE_T0_0_LABEL_1;

    expect(L_TT_CLOSE_PAR);

    // do true block
    _parse_statement(ps);

    JMP_LABEL_2;
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
            expect(L_TT_SEM);
        } break;

        case L_TT_RETURN: {_parse_return(ps);} break;
        case L_TT_WHILE: {_parse_while(ps);} break;
        case L_TT_FOR: {_parse_for(ps);} break;
        case L_TT_BREAK: {_parse_break(ps);} break;
        case L_TT_CONTINUE: {_parse_continue(ps);} break;
        case L_TT_IF: {_parse_if(ps);} break;

        default: {
            P_FATAL("%s: unexpected `%s'", __FUNCTION__, L_ttname(tt));
        } break;
    }
}
