#ifndef lexer_h
#define lexer_h

typedef enum {
    L_TT_INVALID,
    L_TT_EOT,   // end of token stream
    
    L_TT_IDENT,

    L_TT_SEM,           // ;
    L_TT_OPEN_PAR,      // (
    L_TT_CLOSE_PAR,     // )
    L_TT_OPEN_BRACKET,  // [
    L_TT_CLOSE_BRACKET, // ]
    L_TT_COMMA,         // ,
    L_TT_OPEN_BRACE,    // {
    L_TT_CLOSE_BRACE,   // }

    // keywords
    L_TT_VAR,
    L_TT_TRUE,
    L_TT_FALSE,
    L_TT_IF,
    L_TT_ELSE,
    L_TT_BREAK,
    L_TT_CONTINUE,
    L_TT_FOR,
    L_TT_WHILE,
    L_TT_FUNC,
    L_TT_RETURN,

    // types
    L_TT_INT,
    L_TT_FLOAT,
    L_TT_STRING,

    L_TT_OP_ASS,        // =

    // arithmetic
    L_TT_OP_ADD,        // +
    L_TT_OP_SUB,        // -
    L_TT_OP_MUL,        // *
    L_TT_OP_DIV,        // /
    L_TT_OP_MOD,        // %
    L_TT_OP_EXP,        // ^
    L_TT_OP_CONCAT,     // $
    L_TT_OP_INC,        // ++
    L_TT_OP_DEC,        // --
    L_TT_OP_ADDASS,     // +=
    L_TT_OP_SUBASS,     // -=
    L_TT_OP_MULASS,     // *=
    L_TT_OP_DIVASS,     // /=
    L_TT_OP_MODASS,     // %=
    L_TT_OP_EXPASS,     // ^=

    // bitwise
    L_TT_OP_BIT_AND,            // &
    L_TT_OP_BIT_OR,             // |
    L_TT_OP_BIT_XOR,            // #
    L_TT_OP_BIT_NOT,            // ~
    L_TT_OP_BIT_SLEFT,          // <<
    L_TT_OP_BIT_SRIGHT,         // >>
    L_TT_OP_BIT_ANDASS,         // &=
    L_TT_OP_BIT_ORASS,          // |=
    L_TT_OP_BIT_XORASS,         // #=
    L_TT_OP_BIT_SLEFTASS,       // <<=
    L_TT_OP_BIT_SRIGHTASS,      // >>=

    // logic
    L_TT_OP_LOG_AND,    // &&
    L_TT_OP_LOG_OR,     // ||
    L_TT_OP_LOG_NOT,    // !
    L_TT_OP_LOG_EQ,     // ==
    L_TT_OP_LOG_NEQ,    // !=

    // relational
    L_TT_OP_L,  // <
    L_TT_OP_G,  // >
    L_TT_OP_LE, // <=
    L_TT_OP_GE, // >=
} L_TokenType;

typedef struct {
    L_TokenType type;
    union {
        int n;
        double f;
        const char *s;
    } u;
} L_Token;

typedef struct {
    const char *source;
    long curidx;
    int curline;

    int cached;
    L_Token curtoken;
} L_State;

void L_printtoken(const L_Token *t);

L_State* L_newstate(const char *source);
void L_freestate(L_State *Ls);
void L_resetstate(L_State *Ls);

L_TokenType L_nexttoken(L_State *Ls);
void L_cachenexttoken(L_State *Ls);

#endif
