#ifndef parser_h
#define parser_h

#include "list.h"
#include "lexer.h"

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
    list *icodes;
} P_Func;

typedef struct {
    L_State *ls;

    list *symbols;
    list *funcs;
    list *strs;
} P_State;

P_State* P_newstate(L_State *ls);
void P_freestate(P_State *ps);
void P_parse(P_State *ps);

#endif
