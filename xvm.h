#ifndef xvm_h
#define xvm_h

#include <stdio.h>
#include "xasm.h"

typedef struct {
    A_OpType type;
    union {
        int n;
        double f;
        char *s;
    } u;
    int idx;    // for A_OT_ABS_SIDX and A_OT_REL_SIDX and FrameIndex for function call
} V_Value;

typedef struct {
    V_Value *nodes;
    int size;
    int top;
    int frame;
} V_Stack;

typedef struct {
    int opcode;
    int opcount;
    V_Value *ops;
} V_Instr;

typedef struct {
    V_Instr *instr;
    int count;
    int ip;
} V_InstrStream;

typedef struct {
    int entry;
    int param;
    int local;
} V_Func;

typedef struct {
    int count;
    char **api;
} V_Api;

typedef struct {
    int major;
    int minor;
    int global;
    int mainidx;

    V_Value ret;
    V_Stack stack;

    V_InstrStream instr;
    V_Func *func;
    V_Api api;

    list *exportapis;
} V_State;

typedef void (*ExportApi)(V_State *vs);
typedef struct {
    const char *name;
    ExportApi fn;
} V_ExportApi;

V_State* V_newstate();
void V_freestate(V_State *Vs);
void V_loadapis(V_State *vs);
int V_load(V_State *Vs, FILE *f);
void V_run(V_State *Vs);

#endif
