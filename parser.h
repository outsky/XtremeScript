#ifndef parser_h
#define parser_h

#include "list.h"
#include "lexer.h"

#define P_VERSION_MAJOR 0
#define P_VERSION_MINOR 1

#define MAX_SYMBOL_LEN 32

typedef struct {
    char name[MAX_SYMBOL_LEN];
    int size;
    int scope;
    int isparam;
} P_Symbol;

typedef struct {
    char name[MAX_SYMBOL_LEN];
    int param;
    int ishost;
    list *icodes;
} P_Func;

typedef struct {
    struct {
        int major;
        int minor;
    } version;
    L_State *ls;
    int curfunc;

    list *symbols;
    list *funcs;
    list *strs;
} P_State;

P_State* P_newstate(L_State *ls);
void P_freestate(P_State *ps);
void P_parse(P_State *ps);

void P_add_func_icode(P_State *ps, int fidx, void *icode);

#endif
