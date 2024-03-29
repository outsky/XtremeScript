#include <time.h>
#include <string.h>
#include "lib.h"
#include "emitter.h"
#include "icode.h"
#include "parser.h"

#define E_OUTPUT(buff, stream) fwrite(buff, sizeof(char), strlen(buff), stream)

static void _header(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; XSC version %d.%d\n", ps->version.major, ps->version.minor);
    E_OUTPUT(buff, stream);

    time_t now;
    time(&now);
    sprintf(buff, "; Timestamp: %s\n", ctime(&now));
    E_OUTPUT(buff, stream);

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

static void _globals(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; ---- Globals ------------------------\n");
    E_OUTPUT(buff, stream);

    for (const lnode *n = ps->symbols->head; n != NULL; n = n->next) {
        const P_Symbol *sb = (P_Symbol*)n->data;
        if (sb->scope >= 0) {
            continue;
        }

        if (sb->size == 1) {
            sprintf(buff, "Var %s\n", sb->name);
        } else {
            sprintf(buff, "Var %s[%d]\n", sb->name, sb->size);
        }
        E_OUTPUT(buff, stream);
    }

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

static void _func_icode(const P_State *ps, const I_Code *code, FILE *stream) {
    static const char *opnames[] = {
        "MOV", "ADD", "SUB", "MUL", "DIV", "MOD", "EXP", "NEG", "INC", 
        "DEC", "AND", "OR", "XOR", "NOT", "SHL", "SHR", "CONCAT", "GETCHAR", "SETCHAR", 
        "JMP", "JE", "JNE", "JG", "JL", "JGE", "JLE", "PUSH", "POP", "CALL", "RET", 
        "CALLHOST", "PAUSE", "EXIT"
    };
    char buff[256];
    if (code->isjump) {
        sprintf(buff, "Label%d:\n", code->u.jump);
        E_OUTPUT(buff, stream);
        return;
    }

    const I_Instr *instr = &code->u.instr;
    const char* opname = opnames[instr->opcode];
    sprintf(buff, "\t%s", opname);
    E_OUTPUT(buff, stream);

    for (lnode *n = instr->operands->head; n != NULL; n = n->next) {
        I_InstrOperand *opd = (I_InstrOperand*)n->data;
        switch (opd->type) {
            case I_OT_INT: {
                sprintf(buff, " %d", opd->u.n);
            } break;

            case I_OT_FLOAT: {
                sprintf(buff, " %f", opd->u.f);
            } break;

            case I_OT_STRING: {
                int sidx = opd->u.n;
                if (sidx >= ps->strs->count) {
                    error("string index overflow");
                }

                lnode *sn = ps->strs->head;
                for (int i = 0; i < sidx; ++i) {
                    sn = sn->next;
                }
                sprintf(buff, " \"%s\"", (const char*)sn->data);
            } break;

            case I_OT_VAR: {
                P_Symbol *sb = NULL;
                for (lnode *sbn = ps->symbols->head; sbn != NULL; sbn = sbn->next) {
                    sb = (P_Symbol*)sbn->data;
                    if (sb->idx == opd->u.n) {
                        break;
                    }
                }
                if (sb == NULL) {
                    error("var symbol overflow");
                }
                sprintf(buff, " %s", sb->name);
            } break;

            case I_OT_ARRAY_REL: {
                P_Symbol *sb = NULL;
                P_Symbol *sbidx = NULL;
                for (lnode *sbn = ps->symbols->head; sbn != NULL; sbn = sbn->next) {
                    P_Symbol *tmp = (P_Symbol*)sbn->data;
                    if (tmp->idx == opd->u.n) {
                        sb = tmp;
                    }
                    if (tmp->idx == opd->idx) {
                        sbidx = tmp;
                    }
                    if (sb != NULL && sbidx != NULL) {
                        break;
                    }
                }
                if (sb == NULL || sbidx == NULL) {
                    error("array symbol overflow");
                }
                sprintf(buff, " %s[%s]", sb->name, sbidx->name);
            } break;

            case I_OT_FUNCIDX: {
                P_Func *fn = P_get_func_byidx(ps, opd->u.n);
                if (fn == NULL) {
                    error("unknown function called");
                }
                sprintf(buff, " %s", fn->name);
            } break;

            case I_OT_REG: {
                sprintf(buff, " _RetVal");
            } break;

            case I_OT_JUMP: {
                sprintf(buff, " Label%d", opd->u.n);
            } break;

            default: {
                printf("\n[x] %d\n", opd->type);
                error("unhandled operand type");
            } break;
        }
        if (n->next != NULL) {
            strcat(buff, ",");
        }
        E_OUTPUT(buff, stream);
    }
    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

static void _functions(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; ---- Functions ----------------------\n");
    E_OUTPUT(buff, stream);

    int fnidx = 0;
    for (const lnode *n = ps->funcs->head; n != NULL; n = n->next) {
        const P_Func *fn = (P_Func*)n->data;
        if (!fn->ishost) {
            sprintf(buff, "Func %s\n{\n", fn->name);
            E_OUTPUT(buff, stream);

            if (fn->param != 0) {
                for (const lnode *p = ps->symbols->head; p != NULL; p = p->next) {
                    const P_Symbol *sb = (P_Symbol*)p->data;
                    if (sb->isparam && sb->scope == fnidx) {
                        sprintf(buff, "\tParam %s\n", sb->name);
                        E_OUTPUT(buff, stream);
                    }
                }
                sprintf(buff, "\n");
                E_OUTPUT(buff, stream);
            }

            int local = 0;
            for (const lnode *l = ps->symbols->head; l != NULL; l = l->next) {
                const P_Symbol *sb = (P_Symbol*)l->data;
                if (!sb->isparam && sb->scope == fnidx) {
                    local = 1;
                    if (sb->size == 1) {
                        sprintf(buff, "\tVar %s\n", sb->name);
                    } else {
                        sprintf(buff, "\tVar %s[%d]\n", sb->name, sb->size);
                    }
                    E_OUTPUT(buff, stream);
                }
            }
            if (local) {
                sprintf(buff, "\n");
                E_OUTPUT(buff, stream);
            }

            if (fn->icodes->count == 0) {
                sprintf(buff, "\t; No codes\n");
                E_OUTPUT(buff, stream);
            } else {
                for (const lnode *op = fn->icodes->head; op != NULL; op = op->next) {
                    _func_icode(ps, op->data, stream);
                }
            }

            sprintf(buff, "}\n\n");
            E_OUTPUT(buff, stream);
        }

        ++fnidx;
    }

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

void E_emit(P_State *ps, FILE *stream) {
    _header(ps, stream);
    _globals(ps, stream);
    _functions(ps, stream);
}
