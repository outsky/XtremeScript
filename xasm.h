#ifndef xasm_h
#define xasm_h

#include "list.h"

#define A_ID "XSE0"
#define A_VERSION_MAJOR 0
#define A_VERSION_MINOR 1

#define A_DEFAULT_STACKSIZE 1024

#define A_MAX_FUNC_NAME 32
#define A_MAX_OP_NAME 16
#define A_MAX_SYMBOL_NAME 32
#define A_MAX_LABEL_NAME 32

typedef enum {
    A_TT_INT,
    A_TT_FLOAT,
    A_TT_STRING,
    A_TT_IDENT,
    A_TT_COLON,         // :
    A_TT_OPEN_BRACKET,  // [
    A_TT_CLOSE_BRACKET, // ]
    A_TT_COMMA,         // ,
    A_TT_OPEN_BRACE,    // {
    A_TT_CLOSE_BRACE,   // }
    A_TT_NEWLINE,
    A_TT_INSTR,         // instruction
    A_TT_SETSTACKSIZE,
    A_TT_VAR,
    A_TT_FUNC,
    A_TT_PARAM,
    A_TT_REG_RETVAL,    // _RetVal
    A_TT_INVALID,
    A_TT_EOT,           // end of token stream
} A_TokenType;

typedef enum {
    A_OT_INT,
    A_OT_FLOAT,
    A_OT_STRING,
    A_OT_ABS_SIDX,
    A_OT_REL_SIDX,
    A_OT_INSTRIDX,
    A_OT_FUNCIDX,
    A_OT_APIIDX,
    A_OT_REG,
} A_OpType;

typedef enum {
    A_OTM_INT = 1,
    A_OTM_FLOAT = 1 << 1,
    A_OTM_STRING = 1 << 2,
    A_OTM_MEM = 1 << 3,
    A_OTM_LABEL = 1 << 4,
    A_OTM_FUNC = 1 << 5,
    A_OTM_API = 1 << 6,
    A_OTM_REG = 1 << 7,
} A_OpTypeMask;

typedef struct {
    A_TokenType t;
    union {
        int n;
        double f;
        char *s;
    } u;
} A_Token;

typedef struct {
    int stacksize;
    int globalsize;
    int mainidx;
} A_MainHeader;

typedef struct {
    char *program;
    int curidx;
    int curline;

    int cached;
    A_Token curtoken;

    list *opcodes;

    list *symbols;
    list *labels;

    A_MainHeader mh;
    list *instr;
    list *str;
    list *func;
    list *api;
} A_State;

typedef struct {
    char name[A_MAX_SYMBOL_NAME];
    int size;
    int func;
    int stack;
} A_Symbol;

typedef struct {
    char name[A_MAX_LABEL_NAME];
    int func;
    int instr;
} A_Label;

typedef struct {
    char name[A_MAX_OP_NAME];
    int code;
    int param;
    int *opflags;
} A_Opcode;

typedef struct {
    int idx;
    char name[A_MAX_FUNC_NAME];
    int entry;
    int param;
    int local;
} A_Func;

typedef struct {
    int opcode;
    list *operands;
} A_Instr;

typedef struct {
    A_OpType type;
    union {
        int n;
        double f;
    } u;
} A_InstrOperand;

A_State* A_newstate(char *program);
void A_freestate(A_State *As);
void A_resetstate(A_State *As);

A_TokenType A_nexttoken(A_State *As);
void A_cachenexttoken(A_State *As);

void A_parse(A_State *As);
void A_createbin(A_State *As);

#endif
