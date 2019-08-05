#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "xasm.h"
#include "xvm.h"


/* static function declarations */
static void _pvalue(const V_Value *v);
static void _pstatus(V_State *Vs);

static void _reset(V_State *Vs);
static void _copy(V_Value *dest, const V_Value *src);

// stack
static void _push(V_State *Vs, V_Value v);
static void _pop(V_State *Vs);
static void _pushframe(V_State *Vs, int count);
static void _popframe(V_State *Vs, int count);
static V_Value* _getbyidx(V_State *Vs, int idx);

// op
static V_Value* _getopvalue(V_State *Vs, const V_Instr *ins, int idx);

static V_Func* _getfunc(V_State *Vs, int idx);

/* static function definations */
static void _pvalue(const V_Value *v) {
    switch (v->type) {
        case A_OT_NULL: {
            printf("Snull");
        } break;
        case A_OT_FLOAT: {
            printf("%f", v->u.f);
        } break;
        case A_OT_STRING: {
            printf("\"%s\"", v->u.s);
        } break;
        case A_OT_INT: {
            printf("%d", v->u.n);
        } break;
        case A_OT_ABS_SIDX: {
            if (v->u.n >= 0) {
                printf("<abs> S[%d+%d]", v->u.n, v->idx);
            } else {
                printf("<abs> S[%d-%d]", v->u.n, v->idx);
            }
        } break;
        case A_OT_REL_SIDX: {
            if (v->u.n >= 0) {
                printf("<rel> S[%d+S[%d]]", v->u.n, v->idx);
            } else {
                printf("<rel> S[%d-S[%d]]", v->u.n, v->idx);
            }
        } break;
        case A_OT_REG: {
            printf("_RetVal");
        } break;
        case A_OT_FUNCIDX: {
            printf("Fn[%d]", v->u.n);
        } break;
        default: {
            printf("<%d> S[%d]", v->type, v->u.n);
        } break;
    }
}
static void _pstatus(V_State *Vs) {
    printf("\t<"); _pvalue(&Vs->ret); printf(">\n");
    for (int i = Vs->stack.top - 2; i >= 0; --i) {
        printf("\t%d\t", i); _pvalue(&Vs->stack.nodes[i]); printf("\n");
    }
}

static void _reset(V_State *Vs) {
    Vs->ret.type = A_OT_NULL;
    Vs->stack.nodes = (V_Value*)malloc(sizeof(V_Value) * Vs->stack.size);
    for (int i = 0; i < Vs->stack.size; ++i) {
        Vs->stack.nodes[i].type = A_OT_NULL;
    }
    Vs->stack.top = 0;
    Vs->stack.frame = 0;

    _pushframe(Vs, Vs->global);

    Vs->instr.ip = -1;
    V_Func *fn = _getfunc(Vs, Vs->mainidx);
    if (fn != NULL) {
        Vs->instr.ip = fn->entry;
        _pushframe(Vs, fn->local + 1);
        Vs->stack.frame = Vs->stack.top - 1;
    }
}
static void _push(V_State *Vs, V_Value v) {
    _copy(&Vs->stack.nodes[Vs->stack.top++], &v);
}
static void _pop(V_State *Vs) {
    --Vs->stack.top;
}
static void _pushframe(V_State *Vs, int count) {
    Vs->stack.top += count;
}
static void _popframe(V_State *Vs, int count) {
    Vs->stack.top -= count;
}
static V_Value* _getbyidx(V_State *Vs, int idx) {
    int absidx = idx >= 0 ? idx : Vs->stack.frame + idx;
    return &Vs->stack.nodes[absidx];
}
static V_Func* _getfunc(V_State *Vs, int idx) {
    if (idx < 0) {
        return NULL;
    }
    return &Vs->func[idx];
}
static void _copy(V_Value *dest, const V_Value *src) {
    if (dest->type == A_OT_STRING) {
        free(dest->u.s);
    }
    *dest = *src;
    if (dest->type == A_OT_STRING) {
        dest->u.s = strdup(src->u.s);
    }
}
static V_Value* _getopvalue(V_State *Vs, const V_Instr *ins, int idx) {
    V_Value *v = &ins->ops[idx];
    switch (v->type) {
        case A_OT_ABS_SIDX: {
            int idx = v->u.n >= 0 ? v->u.n + v->idx : v->u.n - v->idx;
            return _getbyidx(Vs, idx);
        }

        case A_OT_REL_SIDX: {
            V_Value *var = _getbyidx(Vs, v->idx);
            int idx = v->u.n >= 0 ? v->u.n + var->u.n : v->u.n - var->u.n;
            return _getbyidx(Vs, idx);
        }

        case A_OT_REG: {
            return &Vs->ret;
        }

        default: {
            return v;
        }
    }
}


/* none static functions */

V_State* V_newstate() {
    V_State *vs = (V_State*)malloc(sizeof(*vs));
    memset(vs, 0, sizeof(*vs));
    return vs;
}

void V_freestate(V_State *Vs) {
    free(Vs->stack.nodes);

    for (int i = 0; i < Vs->instr.count; ++i) {
        V_Instr *ins = Vs->instr.instr + i;
        free(ins->ops);
    }
    free(Vs->instr.instr);

    free(Vs->func);

    for (int i = 0; i < Vs->api.count; ++i) {
        free((void*)Vs->api.api[i]);
    }

    free(Vs);
}

int V_load(V_State *Vs, FILE *f) {
    printf("XVM\nWritten by Outsky\n\nLoading...\n\n");

    // header
    char tmp[4];
    fread(tmp, 1, 4, f);
    if (strncmp(A_ID, tmp, 4) != 0) {
        printf("file format not support\n");
        return 0;
    }
    fread(&Vs->major, 1, 1, f);
    fread(&Vs->minor, 1, 1, f);
    fread(&Vs->stack.size, 4, 1, f);
    fread(&Vs->global, 4, 1, f);
    int n = 0;
    fread(&n, 1, 1, f);
    fread(&Vs->mainidx, 4, 1, f);
    if (n != 1) {
        Vs->mainidx = -1;
    }

    // instruction
    Vs->instr.ip = 0;
    fread(&Vs->instr.count, 4, 1, f);
    Vs->instr.instr = (V_Instr*)malloc(sizeof(V_Instr) * Vs->instr.count);
    memset(Vs->instr.instr, 0, sizeof(V_Instr) * Vs->instr.count);
    for (int i = 0; i < Vs->instr.count; ++i) {
        V_Instr *ins = Vs->instr.instr + i;
        fread(&ins->opcode, 2, 1, f);
        fread(&ins->opcount, 1, 1, f);
        ins->ops = (V_Value*)malloc(sizeof(V_Value) * ins->opcount);
        for (int j = 0; j < ins->opcount; ++j) {
            V_Value *v = ins->ops + j;
            v->type = 0;
            fread(&v->type, 1, 1, f);
            if (v->type == A_OT_FLOAT) {
                fread(&v->u.f, sizeof(v->u.f), 1, f);
            } else {
                fread(&v->u.n, sizeof(v->u.n), 1, f);
            }
            if(v->type == A_OT_ABS_SIDX || v->type == A_OT_REL_SIDX) {
                fread(&v->idx, sizeof(v->idx), 1, f);
            }
        }
    }

    // string
    n = 0;
    fread(&n, 4, 1, f);
    int sl = 0;
    char *tmpstr = NULL;
    for (int i = 0; i < n; ++i) {
        sl = 0;
        fread(&sl, 4, 1, f);
        tmpstr = (char*)malloc(sizeof(char) * (sl + 1));
        fread(tmpstr, sizeof(char), sl, f);
        tmpstr[sl] = '\0';
        for (int j = 0; j < Vs->instr.count; ++j) {
            V_Instr *ins = Vs->instr.instr + j;
            for (int k = 0; k < ins->opcount; ++k) {
                V_Value *v = ins->ops + k;
                if (v->type == A_OT_STRING && v->u.n == i) {
                    v->u.s = strdup(tmpstr);
                }
            }
        }
        free(tmpstr);
    }

    // function
    n = 0;
    fread(&n, 4, 1, f);
    Vs->func = (V_Func*)malloc(sizeof(V_Func) * n);
    memset(Vs->func, 0, sizeof(V_Func) * n);
    for (int i = 0; i < n; ++i) {
        V_Func *fn = Vs->func + i;
        fread(&fn->entry, 4, 1, f);
        fread(&fn->param, 1, 1, f);
        fread(&fn->local, 4, 1, f);
    }

    // api
    fread(&Vs->api.count, 4, 1, f);
    Vs->api.api = (char**)malloc(sizeof(char*) * Vs->api.count);
    for (int i = 0; i < Vs->api.count; ++i) {
        fread(&n, 1, 1, f);
        Vs->api.api[i] = (char*)malloc(sizeof(char) * (n + 1));
        fread(Vs->api.api[i], sizeof(char), n, f);
        Vs->api.api[i][n] = '\0';
    }

    if (ferror(f)) {
        printf("load finished with ferror\n");
        return 0;
    }

    printf("load successfully!\n\n");
    printf("Load size: %ld\n", ftell(f));
    printf("XtremeScript VM Version %d.%d\n", Vs->major, Vs->minor);
    printf("Stack Size: %d\n", Vs->stack.size);
    printf("Instructions Assembled: %d\n", Vs->instr.count);
    printf("Globals: %d\n", Vs->global);
    printf("Host API Calls: %d\n", Vs->api.count);
    printf("_Main() Present: %s", Vs->mainidx >= 0 ? "YES" : "No");
    if (Vs->mainidx >= 0) {
        printf(" (Index %d)", Vs->mainidx);
    }

    return 1;
}

void V_run(V_State *Vs) {
    printf("\n\nrunning...\n\n");
    if (Vs->mainidx < 0) {
        printf("No _Main(), nothing to run\n");
        return;
    }

    _reset(Vs);

    for (;;) {
        int ip = Vs->instr.ip;
        if (ip >= Vs->instr.count) {
            break;
        }

        const V_Instr *ins = Vs->instr.instr + ip;
        switch (ins->opcode) {
            case A_OP_MOV: {
                printf("MOV: ");
                V_Value *dest = _getopvalue(Vs, ins, 0);
                V_Value *src = _getopvalue(Vs, ins, 1);
                _pvalue(&ins->ops[0]); printf(", "), _pvalue(src); printf("\n");
                _copy(dest, src);
                _pstatus(Vs);
            } break;

            case A_OP_ADD: {
                printf("ADD: ");
                V_Value *dest = _getopvalue(Vs, ins, 0);
                V_Value *src = _getopvalue(Vs, ins, 1);
                _pvalue(&ins->ops[0]); printf(", "), _pvalue(src); printf("\n");
                if (dest->type != src->type) {
                    fatal(__FUNCTION__, __LINE__, "math ops must have the same type");
                }
                if (dest->type == A_OT_INT) {
                    dest->u.n += src->u.n;
                } else if (dest->type == A_OT_FLOAT) {
                    dest->u.f += src->u.f;
                } else {
                    fatal(__FUNCTION__, __LINE__, "math ops must be int or float");
                }
                _pstatus(Vs);
            } break;

            case A_OP_SUB: {} break;
            case A_OP_MUL: {} break;
            case A_OP_DIV: {} break;
            case A_OP_MOD: {} break;
            case A_OP_EXP: {} break;
            case A_OP_NEG: {} break;
            case A_OP_INC: {} break;
            case A_OP_DEC: {} break;
            case A_OP_AND: {} break;
            case A_OP_OR: {} break;
            case A_OP_XOR: {} break;
            case A_OP_NOT: {} break;
            case A_OP_SHL: {} break;
            case A_OP_SHR: {} break;
            case A_OP_CONCAT: {} break;
            case A_OP_GETCHAR: {} break;
            case A_OP_SETCHAR: {} break;
            case A_OP_JMP: {} break;
            case A_OP_JE: {} break;
            case A_OP_JNE: {} break;
            case A_OP_JG: {} break;
            case A_OP_JL: {} break;
            case A_OP_JGE: {} break;
            case A_OP_JLE: {} break;
            case A_OP_PUSH: {} break;
            case A_OP_POP: {} break;

            case A_OP_CALL: {
                printf("Call: ");
                _pvalue(&ins->ops[0]); printf("\n");

                int idx = ins->ops[0].u.n;
                V_Func *fn = _getfunc(Vs, idx);
                _pushframe(Vs, fn->param);
                V_Value vret;
                vret.type = A_OT_INT;
                vret.u.n = Vs->instr.ip;
                _push(Vs, vret);
                _pushframe(Vs, fn->local);
                V_Value vidx;
                vidx.type = A_OT_INT;
                vidx.u.n = idx;
                _push(Vs, vidx);
                Vs->stack.frame = Vs->stack.top - 1;
                Vs->instr.ip = fn->entry;
            } break;

            case A_OP_RET: {} break;
            case A_OP_CALLHOST: {} break;
            case A_OP_PAUSE: {} break;
            case A_OP_EXIT: {} break;
            default: {} break;
        }
        if (ip == Vs->instr.ip) {
            ++Vs->instr.ip;
        }
    }

    printf("\nrun successfully!\n");
}

