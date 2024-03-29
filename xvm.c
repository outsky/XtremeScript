#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lib.h"
#include "xasm.h"
#include "xvm.h"

//#define V_DEBUG 

/* static function declarations */
static void _freevalue(V_Value *v);

static void _reset(V_State *Vs);
static void _copy(V_Value *dest, const V_Value *src);

// stack
static void _push(V_State *Vs, V_Value v);
static const V_Value* _pop(V_State *Vs);
static void _pushframe(V_State *Vs, int count);
static void _popframe(V_State *Vs, int count);
static V_Value* _getbyidx(V_State *Vs, int idx);
static void _setbyidx(V_State *Vs, int idx, const V_Value* v);

// op
static V_Value* _getopvalue(V_State *Vs, int idx);

static V_Func* _getfunc(V_State *Vs, int idx);
static const char* _getapi(V_State *vs, int idx);
static ExportApi _getexportapi(V_State *Vs, const char *api);

/* static function definations */
static void _freevalue(V_Value *v) {
    if (v->type == A_OT_STRING) {
        free(v->u.s); v->u.s = NULL;
    }
}

#ifdef V_DEBUG
static void _pvalue(const V_Value *v) {
    switch (v->type) {
        case A_OT_NULL: {
            printf("NULL");
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
        case A_OT_INSTRIDX: {
            printf("Instr %d", v->u.n);
        } break;
        default: {
            printf("<%d> %d", v->type, v->u.n);
        } break;
    }
}

static void _pstatus(V_State *Vs) {
    printf("\tIP: %d\n", Vs->instr.ip);
    printf("\t_RetVal: < "); _pvalue(&Vs->ret); printf(" >\n");
    for (int i = Vs->stack.top; i >= 0; --i) {
        if (i == Vs->stack.top) {
            if (Vs->stack.top == Vs->stack.frame) {
                printf("\tTF->\t{ ");
            } else {
                printf("\tT->\t{ ");
            }
        } else if (i == Vs->stack.frame) {
            printf("\tF->\t{ ");
        } else if (i < Vs->global) {
            printf("\tg%d\t{ ", i);
        } else {
            printf("\t%d\t{ ", i);
        }
        _pvalue(&Vs->stack.nodes[i]); printf(" }\n");
    }
}
#endif

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
    }
}
static void _push(V_State *Vs, V_Value v) {
    _copy(&Vs->stack.nodes[Vs->stack.top++], &v);
}
static const V_Value* _pop(V_State *Vs) {
    return &Vs->stack.nodes[--Vs->stack.top];
}
static void _pushframe(V_State *Vs, int count) {
    Vs->stack.top += count;
    Vs->stack.frame = Vs->stack.top;
}
static void _popframe(V_State *Vs, int count) {
    Vs->stack.top -= count;
}
static V_Value* _getbyidx(V_State *Vs, int idx) {
    int absidx = idx >= 0 ? idx : Vs->stack.frame + idx;
    return &Vs->stack.nodes[absidx];
}
static void _setbyidx(V_State *Vs, int idx, const V_Value* v) {
    V_Value* dest = _getbyidx(Vs, idx);
    _copy(dest, v);
}
static V_Func* _getfunc(V_State *Vs, int idx) {
    if (idx < 0) {
        return NULL;
    }
    return &Vs->func[idx];
}
static const char* _getapi(V_State *vs, int idx) {
    if (idx < 0 || idx >= vs->api.count) {
        return NULL;
    }
    return vs->api.api[idx];
}
static ExportApi _getexportapi(V_State *Vs, const char *api) {
    for (lnode *n = Vs->exportapis->head; n != NULL; n = n->next) {
        V_ExportApi *ea = (V_ExportApi*)n->data;
        if (strcmp(ea->name, api) == 0) {
            return ea->fn;
        }
    }
    return NULL;
}
static void _copy(V_Value *dest, const V_Value *src) {
    _freevalue(dest);
    *dest = *src;
    if (dest->type == A_OT_STRING) {
        dest->u.s = strdup(src->u.s);
    }
}
static V_Value* _getopvalue(V_State *Vs, int idx) {
    V_Value *v = &Vs->instr.instr[Vs->instr.ip].ops[idx];
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

static void _run_mov(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *src = _getopvalue(Vs, 1);
    _copy(dest, src);
}

static double _getopvalue_float(V_State *Vs, int idx) {
    const V_Value *v = _getopvalue(Vs, idx);
    if (v->type == A_OT_INT) {
        return (double)v->u.n;
    } else if (v->type == A_OT_FLOAT) {
        return v->u.f;
    }
    error("cant covert to double");
    return 0.0;
}

static int _getopvalue_int(V_State *Vs, int idx) {
    const V_Value *v = _getopvalue(Vs, idx);
    if (v->type == A_OT_INT) {
        return v->u.n;
    } else if (v->type == A_OT_FLOAT) {
        return (int)v->u.f;
    }
    error("type %d cant covert to int", v->type);
    return 0;
}

static void _run_math(V_State *Vs, A_OpCode op) {
    double l = _getopvalue_float(Vs, 0);
    double r = _getopvalue_float(Vs, 1);
    double result = 0.0;

    switch (op) {
        case A_OP_ADD: {result = l + r;} break;
        case A_OP_SUB: {result = l - r;} break;
        case A_OP_MUL: {result = l * r;} break;
        case A_OP_DIV: {result = l / r;} break;
        case A_OP_EXP: {result = pow(l, r);} break;

        default: {
            error("unsupported op");
        }
    }
    
    V_Value *dest = _getopvalue(Vs, 0);
    dest->type = A_OT_FLOAT;
    dest->u.f = result;
}

static void _run_add(V_State *Vs, const V_Instr *ins) {
    _run_math(Vs, A_OP_ADD);
}
static void _run_sub(V_State *Vs, const V_Instr *ins) {
    _run_math(Vs, A_OP_SUB);
}
static void _run_mul(V_State *Vs, const V_Instr *ins) {
    _run_math(Vs, A_OP_MUL);
}
static void _run_div(V_State *Vs, const V_Instr *ins) {
    _run_math(Vs, A_OP_DIV);
}
static void _run_mod(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    int n0 = _getopvalue_int(Vs, 0);
    int n1 = _getopvalue_int(Vs, 1);
    dest->u.n = n0 % n1;
    dest->type = A_OT_INT;
}
static void _run_exp(V_State *Vs, const V_Instr *ins) {
    _run_math(Vs, A_OP_EXP);
}
static void _run_neg(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    if (dest->type == A_OT_INT) {
        dest->u.n = -dest->u.n;
    } else if (dest->type == A_OT_FLOAT) {
        dest->u.f = -dest->u.f;
    } else {
        error("math neg only supports int and float");
    }
}
static void _run_inc(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    ++dest->u.n;
}
static void _run_dec(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    --dest->u.n;
}
static void _run_and(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *src = _getopvalue(Vs, 1);
    dest->u.n &= src->u.n;
}
static void _run_or(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *src = _getopvalue(Vs, 1);
    dest->u.n |= src->u.n;
}
static void _run_xor(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *src = _getopvalue(Vs, 1);
    dest->u.n ^= src->u.n;
}
static void _run_not(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    dest->u.n = ~dest->u.n;
}
static void _run_shl(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *shift = _getopvalue(Vs, 1);
    dest->u.n <<= shift->u.n;
}
static void _run_shr(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *shift = _getopvalue(Vs, 1);
    dest->u.n >>= shift->u.n;
}
static void _run_concat(V_State *Vs, const V_Instr *ins) {
    V_Value *op1 = _getopvalue(Vs, 0);
    V_Value *op2 = _getopvalue(Vs, 1);
    char *buff = (char*)malloc(sizeof(char) * (strlen(op1->u.s) + strlen(op2->u.s) + 1));
    strcpy(buff, op1->u.s);
    strcpy(buff+strlen(buff), op2->u.s);
    free(op1->u.s);
    op1->u.s = buff;
}
static void _run_getchar(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    V_Value *src = _getopvalue(Vs, 1);
    V_Value *idx = _getopvalue(Vs, 2);
    dest->type = A_OT_INT;
    dest->u.n = (int)*(src->u.s + idx->u.n);
}
static void _run_setchar(V_State *Vs, const V_Instr *ins) {
    V_Value *idx = _getopvalue(Vs, 0);
    V_Value *dest = _getopvalue(Vs, 1);
    V_Value *src = _getopvalue(Vs, 2);
    *(dest->u.s + idx->u.n) = (char)src->u.n;
}
static void _run_jmp(V_State *Vs, const V_Instr *ins) {
    Vs->instr.ip = ins->ops[0].u.n;
}
static void _run_jmp_cond(V_State *Vs, const V_Instr *ins, A_OpCode op) {
    int op1 = _getopvalue_int(Vs, 0);
    int op2 = _getopvalue_int(Vs, 1);
    int jump = 0;
    switch (op) {
        case A_OP_JE: {jump = op1 == op2;} break;
        case A_OP_JNE: {jump = op1 != op2;} break;
        case A_OP_JG: {jump = op1 > op2;} break;
        case A_OP_JL: {jump = op1 < op2;} break;
        case A_OP_JGE: {jump = op1 >= op2;} break;
        case A_OP_JLE: {jump = op1 <= op2;} break;
        default: {
            error("invalid condition jump type %d", op);
        } break;
    }
    if (jump) {
        Vs->instr.ip = ins->ops[2].u.n;
    }
}
static void _run_push(V_State *Vs, const V_Instr *ins) {
    V_Value *op = _getopvalue(Vs, 0);
    _push(Vs, *op);
}
static void _run_pop(V_State *Vs, const V_Instr *ins) {
    V_Value *dest = _getopvalue(Vs, 0);
    const V_Value *src = _pop(Vs);
    _copy(dest, src);
}
static void _run_call(V_State *Vs, const V_Instr *ins) {
    int idx = ins->ops[0].u.n;
    V_Func *fn = _getfunc(Vs, idx);
#ifdef V_DEBUG
    printf("(entry %d, param %d, local %d)\n", fn->entry, fn->param, fn->local);
#endif

    V_Value vret;
    vret.type = A_OT_INT;
    vret.u.n = Vs->instr.ip + 1;
    _push(Vs, vret);

    V_Value vidx;
    vidx.type = A_OT_ABS_SIDX;
    vidx.u.n = idx;
    vidx.idx = Vs->stack.frame;

    _pushframe(Vs, fn->local + 1);
    _setbyidx(Vs, -1, &vidx);
    Vs->instr.ip = fn->entry;
}
static void _run_ret(V_State *Vs, const V_Instr *ins) {
    const V_Value *fnidx = _pop(Vs);
    V_Func *fn = &Vs->func[fnidx->u.n];
    V_Value *ret = _getbyidx(Vs, Vs->stack.top - fn->local - 1);
#ifdef V_DEBUG
    printf("(fnidx %d, ret %d, prevframe: %d)\n", fnidx->u.n, ret->u.n, fnidx->idx);
#endif

    Vs->instr.ip = ret->u.n;
    _popframe(Vs, fn->local + fn->param + 1);
    Vs->stack.frame = fnidx->idx;
}

static void _run_callhost(V_State *Vs, const V_Instr *ins) {
    int idx = ins->ops[0].u.n;
    const char *api = _getapi(Vs, idx);
    if (api == NULL) {
        error("host api not found");
    }

    ExportApi fn = _getexportapi(Vs, api);
    fn(Vs);
}

static void _run_pause(V_State *Vs, const V_Instr *ins) {}
static void _run_exit(V_State *Vs, const V_Instr *ins) {
    V_Value *code = _getopvalue(Vs, 0);
    exit(code->u.n);
}

/* none static functions */

V_State* V_newstate() {
    V_State *vs = (V_State*)malloc(sizeof(*vs));
    memset(vs, 0, sizeof(*vs));
    vs->exportapis = list_new();
    return vs;
}

void V_freestate(V_State *Vs) {
    for (int i = 0; i < Vs->stack.size; ++i) {
        _freevalue(&Vs->stack.nodes[i]);
    }
    free(Vs->stack.nodes);

    for (int i = 0; i < Vs->instr.count; ++i) {
        V_Instr *ins = Vs->instr.instr + i;
        for (int j = 0; j < ins->opcount; ++j) {
            V_Value *v = &ins->ops[j];
            if (v->type == A_OT_STRING) {
                free(v->u.s);
            }
        }
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
        if (ins->opcount > 0) {
            ins->ops = (V_Value*)malloc(sizeof(V_Value) * ins->opcount);
        }
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
                V_Value *v = &ins->ops[k];
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
    if (Vs->api.count > 0) {
        Vs->api.api = (char**)malloc(sizeof(char*) * Vs->api.count);
    }
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

#ifdef V_DEBUG
    printf("Load size: %ld\n", ftell(f));
    printf("XtremeScript VM Version %d.%d\n", Vs->major, Vs->minor);
    printf("Stack Size: %d\n", Vs->stack.size);
    printf("Instructions Assembled: %d\n", Vs->instr.count);
    printf("Globals: %d\n", Vs->global);
    printf("Host API Calls: %d\n", Vs->api.count);
    printf("main() Present: %s", Vs->mainidx >= 0 ? "YES" : "No");
    if (Vs->mainidx >= 0) {
        printf(" (Index %d)", Vs->mainidx);
    }
    printf("\n\n");
#endif

    return 1;
}

void V_run(V_State *Vs) {
    _reset(Vs);
    if (Vs->mainidx < 0) {
        printf("No main(), nothing to run\n");
        return;
    }

#ifdef V_DEBUG
    _pstatus(Vs);
#endif
    for (;;) {
        int ip = Vs->instr.ip;
        if (ip >= (Vs->instr.count - 1)) {
            break;
        }

        const V_Instr *ins = Vs->instr.instr + ip;
#ifdef V_DEBUG
        printf("%s: ", A_opnames[ins->opcode]);
        for (int i = 0; i < ins->opcount; ++i) {
            _pvalue(&ins->ops[i]);
            if (i < ins->opcount - 1) {
                printf(", ");
            }
        }
        printf("\n");
#endif
        switch (ins->opcode) {
            case A_OP_MOV: { _run_mov(Vs, ins); } break;
            case A_OP_ADD: { _run_add(Vs, ins); } break;
            case A_OP_SUB: { _run_sub(Vs, ins); } break;
            case A_OP_MUL: { _run_mul(Vs, ins); } break;
            case A_OP_DIV: { _run_div(Vs, ins); } break;
            case A_OP_MOD: { _run_mod(Vs, ins); } break;
            case A_OP_EXP: { _run_exp(Vs, ins); } break;
            case A_OP_NEG: { _run_neg(Vs, ins); } break;
            case A_OP_INC: { _run_inc(Vs, ins); } break;
            case A_OP_DEC: { _run_dec(Vs, ins); } break;
            case A_OP_AND: { _run_and(Vs, ins); } break;
            case A_OP_OR: { _run_or(Vs, ins); } break;
            case A_OP_XOR: { _run_xor(Vs, ins); } break;
            case A_OP_NOT: { _run_not(Vs, ins); } break;
            case A_OP_SHL: { _run_shl(Vs, ins); } break;
            case A_OP_SHR: { _run_shr(Vs, ins); } break;
            case A_OP_CONCAT: { _run_concat(Vs, ins); } break;
            case A_OP_GETCHAR: { _run_getchar(Vs, ins); } break;
            case A_OP_SETCHAR: { _run_setchar(Vs, ins); } break;
            case A_OP_JMP: { _run_jmp(Vs, ins); } break;

            case A_OP_JE: 
            case A_OP_JNE:
            case A_OP_JG:
            case A_OP_JL:
            case A_OP_JGE:
            case A_OP_JLE:{ _run_jmp_cond(Vs, ins, ins->opcode); } break;

            case A_OP_PUSH: { _run_push(Vs, ins); } break;
            case A_OP_POP: { _run_pop(Vs, ins); } break;
            case A_OP_CALL: { _run_call(Vs, ins); } break;
            case A_OP_RET: { _run_ret(Vs, ins); } break;
            case A_OP_CALLHOST: { _run_callhost(Vs, ins); } break;
            case A_OP_PAUSE: { _run_pause(Vs, ins); } break;
            case A_OP_EXIT: { _run_exit(Vs, ins); } break;
            default: { error("unknown op"); } break;
        }
        if (ip == Vs->instr.ip) {
            ++Vs->instr.ip;
        }
#ifdef V_DEBUG
        _pstatus(Vs);
#endif
    }
}


/* export apis */
static void _print(V_State *vs) {
    const V_Value *v = _pop(vs);
    switch (v->type) {
        case A_OT_INT: {
            printf("%d", v->u.n);
        } break;

        case A_OT_FLOAT: {
            printf("%lf", v->u.f);
        } break;

        case A_OT_STRING: {
            printf("%s", v->u.s);
        } break;

        default: {
            error("unsupported optype by _print");
        }
    }
}
static void _println(V_State *vs) {
    const V_Value *v = _pop(vs);
    switch (v->type) {
        case A_OT_INT: {
            printf("%d\n", v->u.n);
        } break;

        case A_OT_FLOAT: {
            printf("%lf\n", v->u.f);
        } break;

        case A_OT_STRING: {
            printf("%s\n", v->u.s);
        } break;

        default: {
            error("unsupported optype by _println");
        }
    }
}

void V_loadapis(V_State *vs) {
    V_ExportApi apis[] = {
        {"print", _print},
        {"println", _println},
        {NULL, NULL}
    };
    
    for (int i = 0;; ++i) {
        V_ExportApi *api = &apis[i];
        if (api->name == NULL) {
            break;
        }

        V_ExportApi *data = (V_ExportApi*)malloc(sizeof(*data));
        data->name = strdup(api->name);
        data->fn = api->fn;
        list_pushback(vs->exportapis, data);
    }
}

