#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "xasm.h"
#include "lib.h"

static const char *_opnames[] = {"MOV", "ADD", "SUB", "MUL", "DIV", "MOD", "EXP", "NEG", "INC", 
    "DEC", "AND", "OR", "XOR", "NOT", "SHL", "SHR", "CONCAT", "GETCHAR", "SETCHAR", 
    "JMP", "JE", "JNE", "JG", "JL", "JGE", "JLE", "PUSH", "POP", "CALL", "RET", 
    "CALLHOST", "PAUSE", "EXIT"};
static int _opcfg[][4] = {
    /*
    param   flag1   flag2   flag3
    */
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_STRING | A_OTM_MEM, 0}, // MOV dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // ADD dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // SUB dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // MUL dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // DIV dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // MOD dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM,  0}, // EXP dest, power
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // NEG dest
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // INC dest
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // DEC dest
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM, 0}, // AND dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM, 0}, // OR  dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM, 0}, // XOR dest, src
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // NOT dest
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM, 0}, // SHL dest, shiftcount
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM, 0}, // SHR dest, shiftcount
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_STRING, 0}, // CONCAT string0, string1
    {3,      A_OTM_REG, A_OTM_STRING, A_OTM_INT}, // GETCHAR dest, src, index
    {3,      A_OTM_INT, A_OTM_REG, A_OTM_STRING}, // SETCHAR index, dest, src
    {1,      A_OTM_LABEL, 0, 0}, // JMP label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JE  op0, op1, label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JNE op0, op1, label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JG  op0, op1, label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JL  op0, op1, label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JGE op0, op1, label
    {3,      A_OTM_MEM, A_OTM_MEM, A_OTM_LABEL}, // JLE op0, op1, label
    {1,      A_OTM_INT | A_OTM_FLOAT | A_OTM_STRING | A_OTM_MEM, 0, 0}, // PUSH src
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // POP  dest
    {1,      A_OTM_FUNC, 0, 0}, // CALL funcname
    {0,      0, 0, 0}, // RET
    {1,      A_OTM_FUNC, 0, 0}, // CALLHOST funcname
    {1,      A_OTM_INT | A_OTM_MEM, 0, 0}, // PAUSE duration
    {1,      A_OTM_INT, 0, 0}, // EXIT  code
};


static void _initops(A_State *As);
static A_Opcode* _newop(const char *name, int code, int param);
static void _setopflag(A_Opcode *op, int idx, int flag);
static A_Opcode* _getop(A_State *As, const char *name);

static int _add_instr(A_State *As, int opcode, list *operands);
static int _add_str(A_State *As, const char *s);
static int _get_stridx(A_State *As, const char *s);
static int _add_func(A_State *As, const char *name, int entry, int param, int local);
static A_Func* _get_func(A_State *As, const char* name);
static A_Func* _get_func_byidx(A_State *As, int idx);
static int _add_api(A_State *As, const char *s);
static int _get_apiidx(A_State *As, const char *s);

static int _add_symbol(A_State *As, const char *name, int size, int func, int stack);
static A_Symbol* _get_symbol(A_State *As, const char *name, int func);
static int _add_label(A_State *As, const char *name, int func, int instr);
static A_Label* _get_label(A_State *As, const char *name, int func);
                    
static A_InstrOperand* _genoperand(A_State *As, int flag, int funcidx);

A_State* A_newstate(char *program) {
    A_State *s = (A_State*)malloc(sizeof(*s));
    s->program = strdup(program);
    s->curidx = 0;
    s->curline = 1;
    s->cached = 0;
    s->curtoken.t = A_TT_INVALID;

    s->opcodes = list_new();

    s->symbols = list_new();
    s->labels = list_new();

    s->mh.stacksize = A_DEFAULT_STACKSIZE;
    s->mh.globalsize = 0;
    s->mh.mainidx = -1;

    s->instr = list_new();
    s->str = list_new();
    s->func = list_new();
    s->api = list_new();

    _initops(s);

    return s;
}

void A_freestate(A_State *As) {
    free(As->program);
    list_free(As->opcodes);
    list_free(As->symbols);
    list_free(As->labels);
    list_free(As->instr);
    list_free(As->str);
    list_free(As->func);
    list_free(As->api);
    free(As);
}

void A_resetstate(A_State *As) {
    As->curidx = 0;
    As->curline = 1;
    As->curtoken.t = A_TT_INVALID;
}

enum A_LexState {
    A_LS_INIT,
    A_LS_INT,       // 123
    A_LS_INTDOT,    // 123.
    A_LS_FLOAT,     // 123.123
    A_LS_STR_HALF,  // "abc
    A_LS_IDENT,
    A_LS_COMMENT,   // ; comment
};

void A_cachenexttoken(A_State *As) {
    As->cached = 1;
}

A_TokenType A_nexttoken(A_State *As) {
    if (As->cached) {
        As->cached = 0;
        return As->curtoken.t;
    }
    if (As->curidx >= strlen(As->program)) {
        return A_TT_EOT;
    }
    int ls = A_LS_INIT;
    int begin = 0; // begin token value
    for (;;) {
        char c = As->program[As->curidx++];
        switch (ls) {
            case A_LS_INIT: {
                if (isblank(c))  {
                    break;
                }

                if (isdigit(c)) {
                    begin = As->curidx - 1;
                    ls = A_LS_INT;
                    break;
                }

                if (c == '_' || isalpha(c)) {
                    begin = As->curidx - 1;
                    ls = A_LS_IDENT;
                    break;
                }

                if (c == '"') {
                    begin = As->curidx;
                    ls = A_LS_STR_HALF;
                    break;
                }

                if (c == ';') {
                    ls = A_LS_COMMENT;
                    break;
                }

                if (c == ':') {As->curtoken.t = A_TT_COLON; return A_TT_COLON;}
                if (c == '[') {As->curtoken.t = A_TT_OPEN_BRACKET; return A_TT_OPEN_BRACKET;}
                if (c == ']') {As->curtoken.t = A_TT_CLOSE_BRACKET; return A_TT_CLOSE_BRACKET;}
                if (c == ',') {As->curtoken.t = A_TT_COMMA; return A_TT_COMMA;}
                if (c == '{') {As->curtoken.t = A_TT_OPEN_BRACE; return A_TT_OPEN_BRACE;}
                if (c == '}') {As->curtoken.t = A_TT_CLOSE_BRACE; return A_TT_CLOSE_BRACE;}
                if (c == '\n') {As->curtoken.t = A_TT_NEWLINE; ++As->curline; return A_TT_NEWLINE;}

                FATAL("invalid char");
            }
            case A_LS_INT: {
                if (isdigit(c)) {
                    break;
                }

                if (c == '.') {
                    ls = A_LS_INTDOT;
                    break;
                }

                if (c == ']' || isspace(c)) {
                    --As->curidx;
                    As->curtoken.t = A_TT_INT;
                    char *tmp = strndup(As->program + begin, As->curidx - begin);
                    As->curtoken.u.n = atoi(tmp);
                    free(tmp);
                    return A_TT_INT;
                }

                printf("\nunexpected char: %c(%d)\n", c, (int)c);
                FATAL("unexpect char");
            }
            case A_LS_INTDOT: {
                if (isdigit(c)) {
                    ls = A_LS_FLOAT;
                    break;
                }
                FATAL("unexpect char");
            }
            case A_LS_FLOAT: {
                if (isdigit(c)) {
                    break;
                }
                
                if (isspace(c)) {
                    --As->curidx;
                    As->curtoken.t = A_TT_FLOAT;
                    char *tmp = strndup(As->program + begin, As->curidx - begin);
                    As->curtoken.u.f = atof(tmp);
                    free(tmp);
                    return A_TT_FLOAT;
                }

                FATAL("unexpect char");
            }
            case A_LS_STR_HALF: {   // TODO: deal with escape \"
                if (c == '"') {
                    As->curtoken.t = A_TT_STRING;
                    As->curtoken.u.s = strndup(As->program + begin, As->curidx - begin - 1);
                    return A_TT_STRING;
                }
                break;
            }
            case A_LS_IDENT: {
                if (c == '[' || c == ',' || c == ':' || isspace(c)) {
                    --As->curidx;

                    char *tmp = strndup(As->program + begin, As->curidx - begin);
                    if (strcasecmp(tmp, "SETSTACKSIZE") == 0) {free(tmp); As->curtoken.t = A_TT_SETSTACKSIZE; return As->curtoken.t;}
                    if (strcasecmp(tmp, "VAR") == 0) {free(tmp); As->curtoken.t = A_TT_VAR; return As->curtoken.t;}
                    if (strcasecmp(tmp, "FUNC") == 0) {free(tmp); As->curtoken.t = A_TT_FUNC; return As->curtoken.t;}
                    if (strcasecmp(tmp, "PARAM") == 0) {free(tmp); As->curtoken.t = A_TT_PARAM; return As->curtoken.t;}
                    if (strcasecmp(tmp, "_RETVAL") == 0) {free(tmp); As->curtoken.t = A_TT_REG_RETVAL; return As->curtoken.t;}

                    if (_getop(As, tmp) != NULL) {
                        As->curtoken.t = A_TT_INSTR;
                        As->curtoken.u.s = tmp;
                        return As->curtoken.t;
                    }

                    As->curtoken.t = A_TT_IDENT;
                    As->curtoken.u.s = tmp;
                    return A_TT_IDENT;
                }
                break;
            }
            case A_LS_COMMENT: {
                if (c == '\n') {
                    --As->curidx;
                    ls = A_LS_INIT;
                    break;
                }
                break;
            }
            default: {
                FATAL("invalid state");
            }
        }
    }
}

static void _parse1(A_State *As) {
    int stacksizeflag = 0;
    int curfunc = -1;
    for (;;) {
        A_TokenType t = A_nexttoken(As);
        if (t == A_TT_INVALID) {
            FATAL("invalid token type");
        }

        switch (t) {
            case A_TT_IDENT: {
                char *s = strdup(As->curtoken.u.s);
                if (A_nexttoken(As) != A_TT_COLON) {
                    free(s);
                    FATAL("label miss colon");
                }
                if (curfunc < 0) {
                    free(s);
                    FATAL("label in global scope is not allowed");
                }
                _add_label(As, s, curfunc, As->instr->count);
                free(s);
                break;
            }
            case A_TT_CLOSE_BRACE: {
                if (curfunc < 0) {
                    FATAL("unexpected `}'");
                }
                A_Opcode *ret = _getop(As, "RET");
                if (ret == NULL) {
                    FATAL("op `ret' not defined");
                }
                _add_instr(As, ret->code, NULL);
                curfunc = -1;
                break;
            }
            case A_TT_NEWLINE: {
                break;
            }
            case A_TT_INSTR: {
                if (curfunc < 0) {
                    FATAL("instr in global scope is not allowed");
                }
                A_Opcode *op = _getop(As, As->curtoken.u.s);
                if (op == NULL) {
                    FATAL("op not registered");
                }
                list *operands = list_new();
                for (int i = 0; i < op->param; ++i) {
                    A_nexttoken(As);
                    int flag = op->opflags[i];
                    A_InstrOperand *iop = _genoperand(As, flag, curfunc);
                    if (iop == NULL) {
                        const char *snap = snapshot(As->program, As->curidx, 32);
                        printf("'''\n%s\n'''\n", snap);
                        free((void*)snap);
                        printf("line %d: op %s param %d, type %d, flag %d, func %d\n", As->curline, 
                            op->name, i, As->curtoken.t, flag, curfunc);
                        FATAL("instr operand type error");
                    }
                    list_pushback(operands, iop);

                    if (i != op->param - 1) {
                        if (A_nexttoken(As) != A_TT_COMMA) {
                            FATAL("missing `,' for instr params");
                        }
                    }
                }
                _add_instr(As, op->code, operands);
                break;
            }
            case A_TT_SETSTACKSIZE: {
                if (stacksizeflag) {
                    FATAL("Cant set stack size more than once");
                }
                if (curfunc > 0) {
                    FATAL("Cant set stack size in function");
                }
                t = A_nexttoken(As);
                if (t != A_TT_INT) {
                    FATAL("`SETSTACKSIZE' must have an integer param");
                }
                As->mh.stacksize = As->curtoken.u.n;
                stacksizeflag = 1;
                break;
            }

            case A_TT_VAR: {
                if (A_nexttoken(As) != A_TT_IDENT) {
                    FATAL("var expect ident");
                }
                char *id = strdup(As->curtoken.u.s);

                if (A_nexttoken(As) == A_TT_OPEN_BRACKET) {
                    if (A_nexttoken(As) != A_TT_INT) {
                        free(id);
                        FATAL("array must have int index");
                    }
                    int n = As->curtoken.u.n;

                    if (A_nexttoken(As) != A_TT_CLOSE_BRACKET) {
                        free(id);
                        FATAL("`]' is missing for array");
                    }

                    if (curfunc < 0) {
                        _add_symbol(As, id, n, curfunc, As->mh.globalsize);
                        As->mh.globalsize += n;
                    } else {
                        A_Func *f = _get_func_byidx(As, curfunc);
                        if (f == NULL) {
                            FATAL("func not found");
                        }
                        _add_symbol(As, id, n, curfunc, -2 - f->local);
                        f->local += n;
                    }
                    free(id);
                    break;
                } else {
                    if (curfunc < 0) {
                        _add_symbol(As, id, 1, curfunc, As->mh.globalsize);
                        As->mh.globalsize += 1;
                    } else {
                        A_Func *f = _get_func_byidx(As, curfunc);
                        if (f == NULL) {
                            printf("curfunc: %d\n", curfunc);
                            FATAL("func not found");
                        }
                        _add_symbol(As, id, 1, curfunc, -2 - f->local);
                        f->local += 1;
                    }

                    A_cachenexttoken(As);
                    free(id);
                    break;
                }
            }
            case A_TT_FUNC: {
                if (curfunc > 0) {
                    FATAL("nested func is not allowed");
                }
                if (A_nexttoken(As) != A_TT_IDENT) {
                    FATAL("func missing ident");
                }
                char *name = strdup(As->curtoken.u.s);

                while (A_nexttoken(As) == A_TT_NEWLINE) {}
                if (As->curtoken.t != A_TT_OPEN_BRACE) {
                    FATAL("func expects `{'");
                }
                curfunc = _add_func(As, name, As->instr->count, 0, 0);
                if (strcmp("_Main", name) == 0) {
                    As->mh.mainidx = curfunc;
                }
                free(name);
                break;
            }
            case A_TT_PARAM: {
                if (curfunc < 0) {
                    FATAL("param in global scope is not allowed");
                }
                if (A_nexttoken(As) != A_TT_IDENT) {
                    FATAL("param expects ident");
                }
                // add to symbol table in the second pass due to lack of function local count
                break;
            }
            case A_TT_EOT: {
                if (curfunc > 0) {
                    FATAL("func not completed");
                }
                return;
            }
            default: {
                FATAL("syntax error");
            }
        }
    }
}

static void _parse2(A_State *As) {}

void A_parse(A_State *As) {
    _parse1(As);
    A_resetstate(As);
    _parse2(As);
}

void A_createbin(A_State *As) {
    printf("-----symbols----\n");
    for (lnode *n = As->symbols->head; n != NULL; n = n->next) {
        A_Symbol *s = (A_Symbol*)n->data;
        printf("%s: size %d, func %d, stack %d\n", s->name, s->size, s->func, s->stack);
    }

    printf("\n-----labels----\n");
    for (lnode *n = As->labels->head; n != NULL; n = n->next) {
        A_Label *l = (A_Label*)n->data;
        printf("%s: func %d, instr %d\n", l->name, l->func, l->instr);
    }

    printf("\n-----header-----\n");
    printf("stacksize: %d\nglobalsize:%d\nmainidx:%d\n", As->mh.stacksize, As->mh.globalsize, As->mh.mainidx);

    printf("\n-----instr----\n");
    for (lnode *n = As->instr->head; n != NULL; n = n->next) {
        A_Instr *i = (A_Instr*)n->data;
        printf("%d(%s)\n", i->opcode, _opnames[i->opcode]);
    }

    printf("\n-----str----\n");
    for (lnode *n = As->str->head; n != NULL; n = n->next) {
        printf("%s\n", (const char*)n->data);
    }

    printf("\n-----func----\n");
    for (lnode *n = As->func->head; n != NULL; n = n->next) {
        A_Func *f = (A_Func*)n->data;
        printf("%s: entry %d, param %d, local %d\n", f->name, f->entry, f->param, f->local);
    }

    printf("\n-----api----\n");
    for (lnode *n = As->api->head; n != NULL; n = n->next) {
        printf("%s\n", (const char*)n->data);
    }
}

static int _add_str(A_State *As, const char *s) {
    int idx = _get_stridx(As, s);
    if (idx >= 0) {
        return idx;
    }
    return list_pushback(As->str, (void*)s);
}

static int _get_stridx(A_State *As, const char *s) {
    int idx = -1;
    for (lnode *n = As->str->head; n != NULL; n = n->next) {
        ++idx;
        if (strcmp(s, (const char*)n->data) == 0) {
            return idx;
        }
    }
    return -1;
}

static int _add_api(A_State *As, const char *s) {
    return list_pushback(As->api, (void*)s);
}

static int _get_apiidx(A_State *As, const char *s) {
    int idx = -1;
    for (lnode *n = As->api->head; n != NULL; n = n->next) {
        ++idx;
        if (strcmp(s, (const char*)n->data) == 0) {
            return idx;
        }
    }
    return -1;
}


static int _add_func(A_State *As, const char *name, int entry, int param, int local) {
    if (_get_func(As, name) != NULL) {
        FATAL("func name exists");
    }
    A_Func *f = (A_Func*)malloc(sizeof(*f));
    strncpy(f->name, name, A_MAX_FUNC_NAME);
    f->idx = list_pushback(As->func, f);
    f->entry = entry;
    f->param = param;
    f->local = local;
    return f->idx;
}

static A_Func* _get_func(A_State *As, const char *name) {
    for (lnode *n = As->func->head; n != NULL; n = n->next) {
        A_Func *f = (A_Func*)n->data;
        if (strcmp(name, f->name) == 0) {
            return f;
        }
    }
    return NULL;
}

static A_Func* _get_func_byidx(A_State *As, int idx) {
    if (idx < 0 || idx >= As->func->count) {
        return NULL;
    }
    lnode *n = As->func->head;
    for (int i = 0; i < idx; ++i) {
        n = n->next;
    }
    return (A_Func*)n->data;
}

static int _add_instr(A_State *As, int opcode, list *operands) {
    A_Instr *i = (A_Instr*)malloc(sizeof(*i));
    i->opcode = opcode;
    i->operands = operands;
    return list_pushback(As->instr, i);
}


static void _initops(A_State *As) { 
    for (int i = 0; i < sizeof(_opnames) / sizeof(char*); ++i) {
        const char* name = _opnames[i];
        if (_getop(As, name) != NULL) {
            FATAL("op already exist");
        }
        int *cfg = _opcfg[i];
        A_Opcode *op = _newop(name, i, cfg[0]);
        for (int j = 0; j < 3; ++j) {
            if (j >= op->param) {
                break;
            }
            _setopflag(op, j, cfg[1 + j]);
        }
        list_pushback(As->opcodes, op);
    }
}

static A_Opcode* _newop(const char* name, int code, int param) {
    A_Opcode *op = (A_Opcode*)malloc(sizeof(*op));
    strncpy(op->name, name, A_MAX_OP_NAME);
    op->code = code;
    op->param = param;
    op->opflags = NULL;
    if (param > 0) {
        op->opflags = (int*)malloc(param * sizeof(int));
    }
    return op;
}

static void _setopflag(A_Opcode *op, int idx, int flag) {
    if (op->param <= idx) {
        FATAL("too many opflags");
    }
    op->opflags[idx] = flag;
}

static A_Opcode* _getop(A_State *As, const char *name) {
    for (lnode *n = As->opcodes->head; n != NULL; n = n->next) {
        A_Opcode *op = (A_Opcode*)n->data;
        if (strcasecmp(name, op->name) == 0) {
            return op;
        }
    }
    return NULL;
}

static int _add_symbol(A_State *As, const char *name, int size, int func, int stack) {
    if (_get_symbol(As, name, func) != NULL) {
        FATAL("symbol exists");
    }
    A_Symbol *s = (A_Symbol*)malloc(sizeof(*s));
    strcpy(s->name, name);
    s->size = size;
    s->func = func;
    s->stack = stack;
    return list_pushback(As->symbols, s);
}

static int _add_label(A_State *As, const char *name, int func, int instr) {
    if (_get_label(As, name, func) != NULL) {
        FATAL("label exists");
    }
    A_Label *l = (A_Label*)malloc(sizeof(*l));
    strcpy(l->name, name);
    l->func = func;
    l->instr = instr;
    return list_pushback(As->labels, l);
}

static A_Symbol* _get_symbol(A_State *As, const char *name, int func) {
    for (lnode *n = As->symbols->head; n != NULL; n = n->next) {
        A_Symbol *s = (A_Symbol*)n->data;
        if (strcmp(name, s->name) == 0 && func == s->func) {
            return s;
        }
    }
    return NULL;
}

static A_Label* _get_label(A_State *As, const char *name, int func) {
    for (lnode *n = As->labels->head; n != NULL; n = n->next) {
        A_Label *l = (A_Label*)n->data;
        if (strcmp(name, l->name) == 0 && func == l->func) {
            return l;
        }
    }
    return NULL;
}

static A_InstrOperand* _genoperand(A_State *As, int flag, int funcidx) {
    A_InstrOperand *io = (A_InstrOperand*)malloc(sizeof(*io));

    int mask = 0;
    switch (As->curtoken.t) {
        case A_TT_INT: {
            mask = A_OTM_INT;
            io->type = A_OT_INT;
            io->u.n = As->curtoken.u.n;
            break;
        }
        case A_TT_FLOAT: {
            mask = A_OTM_FLOAT;
            io->type = A_OT_FLOAT;
            io->u.f = As->curtoken.u.f;
            break;
        }
        case A_TT_STRING: {
            mask = A_OTM_STRING;
            io->type = A_OT_STRING;
            io->u.n = _add_str(As, As->curtoken.u.s);
            break;
        }
        case A_TT_IDENT: {
            char *s = As->curtoken.u.s;

            A_Symbol *smb = _get_symbol(As, s, funcidx);
            if (smb != NULL) {
                mask = A_OTM_MEM;
                io->type = smb->stack > 0 ? A_OT_ABS_SIDX : A_OT_REL_SIDX;
                io->u.n = smb->stack;
                break;
            }

            A_Label *lb = _get_label(As, s, funcidx);
            if (lb != NULL) {
                mask = A_OTM_LABEL;
                io->type = A_OT_INSTRIDX;
                io->u.n = lb->instr;
                break;
            }
            
            A_Func *fn = _get_func(As, s);
            if (fn != NULL) {
                mask = A_OTM_FUNC;
                io->type = A_OT_FUNCIDX;
                io->u.n = fn->idx;
                break;
            }


            int apiidx = _get_apiidx(As, s);
            if (apiidx >= 0) {
                mask = A_OTM_API;
                io->type = A_OT_APIIDX;
                io->u.n = apiidx;
                break;
            }

            break;
        }
        case A_TT_REG_RETVAL: {
            mask = A_OTM_REG;
            io->type = A_OT_REG;
            io->u.n = 0;
            break;
        }
        default: {
            free(io);
            return NULL;
        }
    }

    if (!(flag & mask)) {
        free(io);
        return NULL;
    }
    return io;
}

