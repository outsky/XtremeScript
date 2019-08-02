#ifndef xasm_h
#define xasm_h

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

typedef struct {
    A_TokenType t;
    union {
        int n;
        double f;
        char *s;
    } u;
} A_Token;

typedef struct {
    char *program;
    int curidx;
    A_Token curtoken;
} A_State;

A_State* A_newstate(char *program);
void A_freestate(A_State *S);

A_TokenType A_nexttoken(A_State *S);

#endif
