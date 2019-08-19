#include "icode.h"

I_Code* I_newjump(int jump) {
    I_Code *c = (I_Code*)malloc(sizeof(*c));
    c->isjump = 1;
    c->u.jump = jump;
    return c;
}

I_Code* I_newinstr(int opcode, list *operands) {
    I_Code *c = (I_Code*)malloc(sizeof(*c));
    c->isjump = 0;
    c->instr.opcode = opcode;
    c->instr.operands = operands;
    return c;
}
