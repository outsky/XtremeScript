#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "xasm.h"
#include "lib.h"

A_State* A_newstate(char *program) {
    A_State *s = (A_State*)malloc(sizeof(*s));
    s->program = strdup(program);
    s->curidx = 0;
    s->curtoken.t = A_TT_INVALID;
    return s;
}

void A_freestate(A_State *S) {
    free(S->program);
    free(S);
}

enum A_LexState {
    A_LS_INIT,
    A_LS_INT,       // 123
    A_LS_INTDOT,    // 123.
    A_LS_FLOAT,     // 123.123
    A_LS_STR_HALF,  // "abc
    A_LS_IDENT,
    A_LS_COMMENT,   // ; comment
};

A_TokenType A_nexttoken(A_State *S) {
    if (S->curidx >= strlen(S->program)) {
        return A_TT_EOT;
    }
    int ls = A_LS_INIT;
    int begin = 0; // begin token value
    for (;;) {
        char c = S->program[S->curidx++];
        switch (ls) {
            case A_LS_INIT: {
                if (isblank(c))  {
                    break;
                }

                if (isdigit(c)) {
                    begin = S->curidx - 1;
                    ls = A_LS_INT;
                    break;
                }

                if (c == '_' || isalpha(c)) {
                    begin = S->curidx - 1;
                    ls = A_LS_IDENT;
                    break;
                }

                if (c == '"') {
                    begin = S->curidx;
                    ls = A_LS_STR_HALF;
                    break;
                }

                if (c == ';') {
                    ls = A_LS_COMMENT;
                    break;
                }

                if (c == ':') {S->curtoken.t = A_TT_COLON; return A_TT_COLON;}
                if (c == '[') {S->curtoken.t = A_TT_OPEN_BRACKET; return A_TT_OPEN_BRACKET;}
                if (c == ']') {S->curtoken.t = A_TT_CLOSE_BRACKET; return A_TT_CLOSE_BRACKET;}
                if (c == ',') {S->curtoken.t = A_TT_COMMA; return A_TT_COMMA;}
                if (c == '{') {S->curtoken.t = A_TT_OPEN_BRACE; return A_TT_OPEN_BRACE;}
                if (c == '}') {S->curtoken.t = A_TT_CLOSE_BRACE; return A_TT_CLOSE_BRACE;}
                if (c == '\n') {S->curtoken.t = A_TT_NEWLINE; return A_TT_NEWLINE;}

                fatal(__FUNCTION__, __LINE__, "invalid char");
            }
            case A_LS_INT: {
                if (isdigit(c)) {
                    break;
                }

                if (c == '.') {
                    ls = A_LS_INTDOT;
                    break;
                }

                if (c == ']' || isspace(c)) {
                    --S->curidx;
                    S->curtoken.t = A_TT_INT;
                    char *tmp = strndup(S->program + begin, S->curidx - begin);
                    S->curtoken.u.n = atoi(tmp);
                    free(tmp);
                    return A_TT_INT;
                }

                printf("\nunexpected char: %c(%d)\n", c, (int)c);
                fatal(__FUNCTION__, __LINE__, "unexpect char");
            }
            case A_LS_INTDOT: {
                if (isdigit(c)) {
                    ls = A_LS_FLOAT;
                    break;
                }
                fatal(__FUNCTION__, __LINE__, "unexpect char");
            }
            case A_LS_FLOAT: {
                if (isdigit(c)) {
                    break;
                }
                
                if (isspace(c)) {
                    --S->curidx;
                    S->curtoken.t = A_TT_FLOAT;
                    char *tmp = strndup(S->program + begin, S->curidx - begin);
                    S->curtoken.u.f = atof(tmp);
                    free(tmp);
                    return A_TT_FLOAT;
                }

                fatal(__FUNCTION__, __LINE__, "unexpect char");
            }
            case A_LS_STR_HALF: {   // TODO: deal with escape \"
                if (c == '"') {
                    S->curtoken.t = A_TT_STRING;
                    S->curtoken.u.s = strndup(S->program + begin, S->curidx - begin - 1);
                    return A_TT_STRING;
                }
                break;
            }
            case A_LS_IDENT: {
                if (c == '[' || c == ',' || isspace(c)) {
                    --S->curidx;
                    S->curtoken.t = A_TT_IDENT;
                    S->curtoken.u.s = strndup(S->program + begin, S->curidx - begin);
                    return A_TT_IDENT;
                }
                break;
            }
            case A_LS_COMMENT: {
                if (c == '\n') {
                    --S->curidx;
                    ls = A_LS_INIT;
                    break;
                }
                break;
            }
            default: {
                fatal(__FUNCTION__, __LINE__, "invalid state");
            }
        }
    }
}
