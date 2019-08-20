#include <time.h>
#include <string.h>
#include "emitter.h"

#define E_OUTPUT(buff, stream) fwrite(buff, sizeof(char), strlen(buff), stream)

static void _header(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; XSC version %d.%d\n", ps->version.major, ps->version.minor);
    E_OUTPUT(buff, stream);

    time_t now;
    time(&now);
    sprintf(buff, "; Timestamp: %s\n", ctime(&now));
    E_OUTPUT(buff, stream);

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

static void _globals(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; ---- Globals ------------------------\n");
    E_OUTPUT(buff, stream);

    for (const lnode *n = ps->symbols->head; n != NULL; n = n->next) {
        const P_Symbol *sb = (P_Symbol*)n->data;
        if (sb->scope >= 0) {
            continue;
        }

        if (sb->size == 1) {
            sprintf(buff, "Var %s\n", sb->name);
        } else {
            sprintf(buff, "Var %s[%d]\n", sb->name, sb->size);
        }
        E_OUTPUT(buff, stream);
    }

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

static void _functions(P_State *ps, FILE *stream) {
    char buff[256];
    sprintf(buff, "; ---- Functions ----------------------\n");
    E_OUTPUT(buff, stream);

    int fnidx = 0;
    for (const lnode *n = ps->funcs->head; n != NULL; n = n->next) {
        const P_Func *fn = (P_Func*)n->data;
        if (!fn->ishost) {
            sprintf(buff, "Func %s\n{\n", fn->name);
            E_OUTPUT(buff, stream);

            if (fn->param != 0) {
                for (const lnode *p = ps->symbols->head; p != NULL; p = p->next) {
                    const P_Symbol *sb = (P_Symbol*)p->data;
                    if (sb->isparam && sb->scope == fnidx) {
                        sprintf(buff, "\tParam %s\n", sb->name);
                        E_OUTPUT(buff, stream);
                    }
                }
                sprintf(buff, "\n");
                E_OUTPUT(buff, stream);
            }

            int local = 0;
            for (const lnode *l = ps->symbols->head; l != NULL; l = l->next) {
                const P_Symbol *sb = (P_Symbol*)l->data;
                if (!sb->isparam && sb->scope == fnidx) {
                    local = 1;
                    if (sb->size == 1) {
                        sprintf(buff, "\tVar %s\n", sb->name);
                    } else {
                        sprintf(buff, "\tVar %s[%d]\n", sb->name, sb->size);
                    }
                    E_OUTPUT(buff, stream);
                }
            }
            if (local) {
                sprintf(buff, "\n");
                E_OUTPUT(buff, stream);
            }

            if (fn->icodes->count == 0) {
                sprintf(buff, "\t; No codes\n");
                E_OUTPUT(buff, stream);
            } else {
                // TODO:
            }

            sprintf(buff, "}\n\n");
            E_OUTPUT(buff, stream);
        }

        ++fnidx;
    }

    sprintf(buff, "\n");
    E_OUTPUT(buff, stream);
}

void E_emit(P_State *ps, FILE *stream) {
    _header(ps, stream);
    _globals(ps, stream);
    _functions(ps, stream);
}
