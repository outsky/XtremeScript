#ifndef icode_h
#define icode_h

typedef enum {
    I_OP_MOV, I_OP_ADD, I_OP_SUB, I_OP_MUL, I_OP_DIV, I_OP_MOD, I_OP_EXP, I_OP_NEG, I_OP_INC, 
    I_OP_DEC, I_OP_AND, I_OP_OR, I_OP_XOR, I_OP_NOT, I_OP_SHL, I_OP_SHR, I_OP_CONCAT, I_OP_GETCHAR, I_OP_SETCHAR, 
    I_OP_JMP, I_OP_JE, I_OP_JNE, I_OP_JG, I_OP_JL, I_OP_JGE, I_OP_JLE, I_OP_PUSH, I_OP_POP, I_OP_CALL, I_OP_RET, 
    I_OP_CALLHOST, I_OP_PAUSE, I_OP_EXIT, I_OP_ECHO
} I_OpCode;

typedef enum {
    I_OT_INT,
    I_OT_FLOAT,
    I_OT_STRING,
    I_OT_VAR,
    I_OT_ARRAY_ABS,
    I_OT_ARRAY_REL,
    I_OT_JUMP,
    I_OT_FUNCIDX,
    I_OT_REG,
} I_OpType;

typedef struct {
    int opcode;
    list *operands;
} I_Instr;

typedef struct {
    I_OpType type;
    union {
        int n;
        double f;
    } u;
    int idx;
} I_InstrOperand;

typedef struct {
    int isjump;
    union {
        I_Instr instr;
        int jump;
    } u;
} I_Code;

I_Code* I_newjump(int jump);
I_Code* I_newinstr(int opcode, list *operands);

#endif
