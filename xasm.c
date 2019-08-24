#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "xasm.h"
#include "lib.h"

//#define A_DEBUG

#define A_FATAL(msg) printf("<line: %d>\n", As->curline); snapshot(As->program, As->curidx); fatal(__FILE__, __LINE__, msg)

const char *A_opnames[] = {"MOV", "ADD", "SUB", "MUL", "DIV", "MOD", "EXP", "NEG", "INC", 
    "DEC", "AND", "OR", "XOR", "NOT", "SHL", "SHR", "CONCAT", "GETCHAR", "SETCHAR", 
    "JMP", "JE", "JNE", "JG", "JL", "JGE", "JLE", "PUSH", "POP", "CALL", "RET", 
    "CALLHOST", "PAUSE", "EXIT"};


static int _opcfg[][4] = {
    /*
    param   flag1   flag2   flag3
    */
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_STRING | A_OTM_MEM | A_OTM_REG, 0}, // MOV dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // ADD dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // SUB dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // MUL dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // DIV dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // MOD dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_FLOAT | A_OTM_MEM | A_OTM_REG,  0}, // EXP dest, power
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // NEG dest
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // INC dest
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // DEC dest
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_MEM, 0}, // AND dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_MEM, 0}, // OR  dest, src
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_MEM, 0}, // XOR dest, src
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // NOT dest
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_MEM, 0}, // SHL dest, shiftcount
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_INT | A_OTM_MEM, 0}, // SHR dest, shiftcount
    {2,      A_OTM_MEM | A_OTM_REG, A_OTM_STRING, 0}, // CONCAT string0, string1
    {3,      A_OTM_REG, A_OTM_STRING, A_OTM_INT}, // GETCHAR dest, src, index
    {3,      A_OTM_INT, A_OTM_REG, A_OTM_STRING}, // SETCHAR index, dest, src
    {1,      A_OTM_LABEL, 0, 0}, // JMP label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JE  op0, op1, label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JNE op0, op1, label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JG  op0, op1, label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JL  op0, op1, label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JGE op0, op1, label
    {3,      A_OTM_MEM | A_OTM_REG, A_OTM_MEM | A_OTM_INT, A_OTM_LABEL}, // JLE op0, op1, label
    {1,      A_OTM_INT | A_OTM_FLOAT | A_OTM_STRING | A_OTM_MEM | A_OTM_REG, 0, 0}, // PUSH src
    {1,      A_OTM_MEM | A_OTM_REG, 0, 0}, // POP dest
    {1,      A_OTM_FUNC, 0, 0}, // CALL funcname
    {0,      0, 0, 0}, // RET
    {1,      A_OTM_API, 0, 0}, // CALLHOST funcname
    {1,      A_OTM_INT | A_OTM_MEM, 0, 0}, // PAUSE duration
    {1,      A_OTM_INT, 0, 0}, // EXIT  code
};

static void _freetoken(A_Token *t);

static void _initops(A_State *As);
static A_Opcode* _newop(const char *name, int code, int param);
static void _setopflag(A_State *As, A_Opcode *op, int idx, int flag);
static const A_Opcode* _getop(A_State *As, const char *name);

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
    _freetoken(&As->curtoken);

    for (lnode *n = As->opcodes->head; n != NULL; n = n->next) {
        A_Opcode *op = (A_Opcode*)n->data;
        free(op->opflags);
    }
    list_free(As->opcodes);

    list_free(As->symbols);
    list_free(As->labels);

    for (lnode *n = As->instr->head; n != NULL; n = n->next) {
        A_Instr *ins = (A_Instr*)n->data;
        list_free(ins->operands);
    }
    list_free(As->instr);

    list_free(As->str);
    list_free(As->func);
    list_free(As->api);
    free(As);
}

void A_resetstate(A_State *As) {
    As->curidx = 0;
    As->curline = 1;
    _freetoken(&As->curtoken);
}

enum A_LexState {
    A_LS_INIT,
    A_LS_UNARY,    // + | -
    A_LS_INT,       // 123
    A_LS_INTDOT,    // 123.
    A_LS_FLOAT,     // 123.123
    A_LS_STR_HALF,  // "abc
    A_LS_STR_ESC,   // "abc\?
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
    _freetoken(&As->curtoken);
    for (;;) {
        char c = As->program[As->curidx++];
        switch (ls) {
            case A_LS_INIT: {
                if (c == '\r' || isblank(c))  {
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

                if (c == '+' || c == '-') {
                    begin = As->curidx - 1;
                    ls = A_LS_UNARY;
                    break;
                }

                if (c == ':') {As->curtoken.t = A_TT_COLON; return A_TT_COLON;}
                if (c == '[') {As->curtoken.t = A_TT_OPEN_BRACKET; return A_TT_OPEN_BRACKET;}
                if (c == ']') {As->curtoken.t = A_TT_CLOSE_BRACKET; return A_TT_CLOSE_BRACKET;}
                if (c == ',') {As->curtoken.t = A_TT_COMMA; return A_TT_COMMA;}
                if (c == '{') {As->curtoken.t = A_TT_OPEN_BRACE; return A_TT_OPEN_BRACE;}
                if (c == '}') {As->curtoken.t = A_TT_CLOSE_BRACE; return A_TT_CLOSE_BRACE;}
                if (c == '\n') {As->curtoken.t = A_TT_NEWLINE; ++As->curline; return A_TT_NEWLINE;}

                printf("%c(%d)\n", c, c);
                A_FATAL("invalid char");
            }

            case A_LS_UNARY: {
                if (!isdigit(c)) {
                    A_FATAL("number expected by unary op");
                }
                ls = A_LS_INT;
            } break;

            case A_LS_INT: {
                if (isdigit(c)) {
                    break;
                }

                if (c == '.') {
                    ls = A_LS_INTDOT;
                    break;
                }

                if (c == ']' || c == ',' || isspace(c)) {
                    --As->curidx;
                    As->curtoken.t = A_TT_INT;
                    char *tmp = strndup(As->program + begin, As->curidx - begin);
                    As->curtoken.u.n = atoi(tmp);
                    free(tmp);
                    return A_TT_INT;
                }

                printf("\nunexpected char: %c(%d)\n", c, (int)c);
                A_FATAL("unexpect char");
            }
            case A_LS_INTDOT: {
                if (isdigit(c)) {
                    ls = A_LS_FLOAT;
                    break;
                }
                A_FATAL("unexpect char");
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

                A_FATAL("unexpect char");
            }
            case A_LS_STR_HALF: {
                if (c == '\\') {
                    ls = A_LS_STR_ESC;
                    break;
                }
                if (c == '"') {
                    As->curtoken.t = A_TT_STRING;
                    As->curtoken.u.s = strndup(As->program + begin, As->curidx - begin - 1);
                    return A_TT_STRING;
                }
                break;
            }
            case A_LS_STR_ESC: {
                ls = A_LS_STR_HALF;
            } break;
            case A_LS_IDENT: {
                if (c != '_' && !isalnum(c)) {
                    --As->curidx;

                    _freetoken(&As->curtoken);

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
                A_FATAL("invalid state");
            }
        }
    }
}

static void _pass1(A_State *As) {
    int stacksizeflag = 0;
    int curfunc = -1;
    int curinstr = 0;
    for (;;) {
        A_TokenType t = A_nexttoken(As);
        if (t == A_TT_INVALID) {
            A_FATAL("invalid token type");
        }

        switch (t) {
            case A_TT_IDENT: {
                char *s = strdup(As->curtoken.u.s);
                if (A_nexttoken(As) != A_TT_COLON) {
                    free(s);
                    A_FATAL("label miss colon");
                }
                if (curfunc < 0) {
                    free(s);
                    A_FATAL("label in global scope is not allowed");
                }
                _add_label(As, s, curfunc, curinstr);
                free(s);
                break;
            }
            case A_TT_CLOSE_BRACE: {
                if (curfunc < 0) {
                    A_FATAL("unexpected `}'");
                }
                ++curinstr;
                curfunc = -1;
                break;
            }
            case A_TT_NEWLINE: {
                break;
            }
            case A_TT_INSTR: {
                if (curfunc < 0) {
                    A_FATAL("instr in global scope is not allowed");
                }
                const A_Opcode *op = _getop(As, As->curtoken.u.s);
                if (op == NULL) {
                    A_FATAL("op not registered");
                }

                if (strcasecmp(op->name, "CALLHOST") == 0) {
                    if (A_nexttoken(As) != A_TT_IDENT) {
                        A_FATAL("ident expected");
                    }
                    _add_api(As, As->curtoken.u.s);
                }

                while (A_nexttoken(As) != A_TT_NEWLINE) {}
                ++curinstr;
                A_cachenexttoken(As);
                break;
            }
            case A_TT_SETSTACKSIZE: {
                if (stacksizeflag) {
                    A_FATAL("Cant set stack size more than once");
                }
                if (curfunc > 0) {
                    A_FATAL("Cant set stack size in function");
                }
                t = A_nexttoken(As);
                if (t != A_TT_INT) {
                    A_FATAL("`SETSTACKSIZE' must have an integer param");
                }
                As->mh.stacksize = As->curtoken.u.n;
                stacksizeflag = 1;
                break;
            }

            case A_TT_VAR: {
                if (A_nexttoken(As) != A_TT_IDENT) {
                    A_FATAL("var expect ident");
                }
                char *id = strdup(As->curtoken.u.s);

                if (A_nexttoken(As) == A_TT_OPEN_BRACKET) {
                    if (A_nexttoken(As) != A_TT_INT) {
                        free(id);
                        A_FATAL("array must have int index");
                    }
                    int n = As->curtoken.u.n;

                    if (A_nexttoken(As) != A_TT_CLOSE_BRACKET) {
                        free(id);
                        A_FATAL("`]' is missing for array");
                    }

                    if (curfunc < 0) {
                        _add_symbol(As, id, n, curfunc, As->mh.globalsize);
                        As->mh.globalsize += n;
                    } else {
                        A_Func *f = _get_func_byidx(As, curfunc);
                        if (f == NULL) {
                            A_FATAL("func not found");
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
                            A_FATAL("func not found");
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
                    A_FATAL("nested func is not allowed");
                }
                if (A_nexttoken(As) != A_TT_IDENT) {
                    A_FATAL("func missing ident");
                }
                char *name = strdup(As->curtoken.u.s);

                while (A_nexttoken(As) == A_TT_NEWLINE) {}
                if (As->curtoken.t != A_TT_OPEN_BRACE) {
                    A_FATAL("func expects `{'");
                }
                curfunc = _add_func(As, name, 0, 0, 0);
                if (strcmp("main", name) == 0) {
                    As->mh.mainidx = curfunc;
                }
                free(name);
                break;
            }
            case A_TT_PARAM: {
                if (curfunc < 0) {
                    A_FATAL("param in global scope is not allowed");
                }
                if (A_nexttoken(As) != A_TT_IDENT) {
                    A_FATAL("param expects ident");
                }
                break;
            }
            case A_TT_EOT: {
                if (curfunc > 0) {
                    A_FATAL("func not completed");
                }
                return;
            }
            default: {
                A_FATAL("syntax error");
            }
        }
    }
}

static void _pass2(A_State *As) {
    int curfunc = -1;
    for (;;) {
        A_TokenType t = A_nexttoken(As);
        switch (t) {
            case A_TT_IDENT: {
                A_nexttoken(As);
                break;
            }
            case A_TT_CLOSE_BRACE: {
                const A_Opcode *ret = _getop(As, "RET");
                if (ret == NULL) {
                    A_FATAL("op `ret' not defined");
                }
                list *operands = list_new();
                _add_instr(As, ret->code, operands);
                curfunc = -1;
                break;
            }
            case A_TT_NEWLINE: {
                break;
            }
            case A_TT_INSTR: {
                const A_Opcode *op = _getop(As, As->curtoken.u.s);
                list *operands = list_new();
                for (int i = 0; i < op->param; ++i) {
                    A_nexttoken(As);
                    int flag = op->opflags[i];
                    A_InstrOperand *iop = _genoperand(As, flag, curfunc);
                    if (iop == NULL) {
                        printf("line %d: op %s param %d, type %d, flag %d, func %d\n",
                            As->curline, op->name, i, As->curtoken.t, flag, curfunc);
                        A_FATAL("instr operand type error");
                    }
 
                    list_pushback(operands, iop);

                    if (i != op->param - 1) {
                        if (A_nexttoken(As) != A_TT_COMMA) {
                            A_FATAL("missing `,' for instr params");
                        }
                    }
                }
                _add_instr(As, op->code, operands);
                break;
            }
            case A_TT_SETSTACKSIZE: {
                A_nexttoken(As);
                break;
            }

            case A_TT_VAR: {
                A_nexttoken(As);
                if (A_nexttoken(As) == A_TT_OPEN_BRACKET) {
                    A_nexttoken(As);
                    A_nexttoken(As);
                } else {
                    A_cachenexttoken(As);
                }
                break;
            }
            case A_TT_FUNC: {
                A_nexttoken(As);
                char *name = strdup(As->curtoken.u.s);
                while (A_nexttoken(As) == A_TT_NEWLINE) {}
                A_Func *fn = _get_func(As, name);
                fn->entry = As->instr->count;
                curfunc = fn->idx;
                free(name);
                break;
            }
            case A_TT_PARAM: {
                A_nexttoken(As);
                A_Func *fn = _get_func_byidx(As, curfunc);
                /*
                frame
                funcidx
                local
                ret
                param
                */
                _add_symbol(As, As->curtoken.u.s, 1, curfunc, -2 - fn->local - 1 - fn->param);
                ++fn->param;
                break;
            }
            case A_TT_EOT: {
                return;
            }
            default: {
                A_FATAL("syntax error");
            }
        }
    }
}

void A_parse(A_State *As) {
    _pass1(As);
    A_resetstate(As);
    _pass2(As);
}

void A_createbin(A_State *As) {
    const char *fname = "a.xse";
    FILE *f = fopen(fname, "wb");

    int num = 0;
    // header
    fwrite(A_ID, 1, 4, f);
    num = A_VERSION_MAJOR;
    fwrite(&num, 1, 1, f);
    num = A_VERSION_MINOR;
    fwrite(&num, 1, 1, f);
    fwrite(&As->mh.stacksize, 4, 1, f);
    fwrite(&As->mh.globalsize, 4, 1, f);
    num = As->mh.mainidx >= 0 ? 1 : 0;
    fwrite(&num, 1, 1, f);
    fwrite(&As->mh.mainidx, 4, 1, f);

    // instruction stream
    fwrite(&As->instr->count, 4, 1, f);
    for (lnode *n = As->instr->head; n != NULL; n = n->next) {
        A_Instr *i = (A_Instr*)n->data;
        fwrite(&i->opcode, 2, 1, f);
        num = _opcfg[i->opcode][0];
        fwrite(&num, 1, 1, f);
        for (lnode *o = i->operands->head; o != NULL; o = o->next) {
            A_InstrOperand *io = (A_InstrOperand*)o->data;
            fwrite(&io->type, 1, 1, f);
            if (io->type == A_OT_ABS_SIDX || io->type == A_OT_REL_SIDX) {
                fwrite(&io->u.n, sizeof(io->u.n), 1, f);
                fwrite(&io->idx, sizeof(io->idx), 1, f);
            } else if (io->type == A_OT_FLOAT) {
                fwrite(&io->u.f, sizeof(io->u.f), 1, f);
            } else {
                fwrite(&io->u.n, sizeof(io->u.n), 1, f);
            }
        }
    }

    // string table
    fwrite(&As->str->count, 4, 1, f);
    for (lnode *n = As->str->head; n != NULL; n = n->next) {
        const char *s = (const char*)n->data;
        num = strlen(s);
        fwrite(&num, 4, 1, f);
        fwrite(s, 1, num, f);
    }

    // function table
    fwrite(&As->func->count, 4, 1, f);
    for (lnode *n = As->func->head; n != NULL; n = n->next) {
        A_Func *fn = (A_Func*)n->data;
        fwrite(&fn->entry, 4, 1, f);
        fwrite(&fn->param, 1, 1, f);
        fwrite(&fn->local, 4, 1, f);
    }

    // api table
    fwrite(&As->api->count, 4, 1, f);
    for (lnode *n = As->api->head; n != NULL; n = n->next) {
        char *s = (char*)n->data;
        num = strlen(s);
        fwrite(&num, 1, 1, f);
        fwrite(s, 1, num, f);
    }

#ifdef A_DEBUG
    long fsize = ftell(f);
#endif
    fclose(f);

    int varcount = 0;
    int arrcount = 0;
    for (lnode *n = As->symbols->head; n != NULL; n = n->next) {
        A_Symbol *sb = (A_Symbol*)n->data;
        if (sb->size == 1) {
            ++varcount;
        } else {
            ++arrcount;
        }
    }

#ifdef A_DEBUG
    printf("Output file: %s\n", fname);
    printf("Version %d.%d\n", A_VERSION_MAJOR, A_VERSION_MINOR);
    printf("Write size: %ld\n", fsize);
    printf("Source Lines Processed: %d\n", As->curline);
    printf("Stack Size: %d\n", As->mh.stacksize);
    printf("Instructions Assembled: %d\n", As->instr->count);
    printf("Variables: %d\n", varcount);
    printf("Arrays: %d\n", arrcount);
    printf("Globals: %d\n", As->mh.globalsize);
    printf("String Literals: %d\n", As->str->count);
    printf("Labels: %d\n", As->labels->count);
    printf("Host API Calls: %d\n", As->api->count);
    printf("Functions: %d\n", As->func->count);
    printf("main() Present: %s", As->mh.mainidx >= 0 ? "YES" : "No");
    if (As->mh.mainidx >= 0) {
        printf(" (Index %d)", As->mh.mainidx);
    }
    printf("\n");
#endif
}

static int _add_str(A_State *As, const char *s) {
    int idx = _get_stridx(As, s);
    if (idx >= 0) {
        return idx;
    }
    const char *snew = strdup(s);
    return list_pushback(As->str, (void*)snew);
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
    int idx = _get_apiidx(As, s);
    if (idx >= 0) {
        return idx;
    }
    return list_pushback(As->api, strdup(s));
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
        A_FATAL("func name exists");
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

static void _freetoken(A_Token *t) {
    if (t->t == A_TT_STRING || t->t == A_TT_IDENT || t->t == A_TT_INSTR) {
        free(t->u.s); t->u.s = NULL;
    }
    t->t = A_TT_INVALID;
}

static void _initops(A_State *As) { 
    for (int i = 0; i < sizeof(A_opnames) / sizeof(char*); ++i) {
        const char* name = A_opnames[i];
        if (_getop(As, name) != NULL) {
            A_FATAL("op already exist");
        }
        int *cfg = _opcfg[i];
        A_Opcode *op = _newop(name, i, cfg[0]);
        for (int j = 0; j < 3; ++j) {
            if (j >= op->param) {
                break;
            }
            _setopflag(As, op, j, cfg[1 + j]);
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

static void _setopflag(A_State *As, A_Opcode *op, int idx, int flag) {
    if (op->param <= idx) {
        A_FATAL("too many opflags");
    }
    op->opflags[idx] = flag;
}

static const A_Opcode* _getop(A_State *As, const char *name) {
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
        A_FATAL("symbol exists");
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
        A_FATAL("label exists");
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
            if (smb == NULL) {
                smb = _get_symbol(As, s, -1);
            }
            if (smb != NULL) {
                mask = A_OTM_MEM;
                io->type = A_OT_ABS_SIDX;
                io->u.n = smb->stack;
                io->idx = 0;

                if (smb->size > 1) {
                    if (A_nexttoken(As) != A_TT_OPEN_BRACKET) {
                        A_FATAL("missing `['");
                    }

                    int tt = A_nexttoken(As);
                    if (tt == A_TT_INT) {
                        io->idx = As->curtoken.u.n;
                    } else if(tt == A_TT_IDENT) {
                        A_Symbol *sidx = _get_symbol(As, As->curtoken.u.s, funcidx);
                        if (sidx == NULL) {
                            sidx = _get_symbol(As, As->curtoken.u.s, -1);
                        }
                        if (sidx == NULL) {
                            A_FATAL("undefined ident");
                        }
                        if (sidx->size != 1) {
                            A_FATAL("expected int or var");
                        }

                        io->type = A_OT_REL_SIDX;
                        io->idx = sidx->stack;
                    } else {
                        A_FATAL("expected int or ident");
                    }
                    if (A_nexttoken(As) != A_TT_CLOSE_BRACKET) {
                        A_FATAL("missing `]'");
                    }
                }
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

