#include <stdlib.h>
#include "icode.h"

I_Code* I_newjump(int jump) {
    I_Code *c = (I_Code*)malloc(sizeof(*c));
    c->isjump = 1;
    c->u.jump = jump;
    return c;
}

I_Code* I_newinstr(I_OpCode opcode) {
    I_Code *c = (I_Code*)malloc(sizeof(*c));
    c->isjump = 0;
    c->u.instr.opcode = opcode;
    c->u.instr.operands = list_new();
    return c;
}

void I_addoperand(I_Code *code, I_OpType type, double f, int idx) {
    I_InstrOperand *opd = (I_InstrOperand*)malloc(sizeof(*opd));
    opd->type = type;
    opd->idx = idx;
    if (type == I_OT_FLOAT) {
        opd->u.f = f;
    } else {
        opd->u.n = (int)f;
    }
    list_pushback(code->u.instr.operands, opd);
}
