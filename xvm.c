#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "xasm.h"
#include "xvm.h"

V_State* V_newstate() {
    V_State *vs = (V_State*)malloc(sizeof(*vs));
    memset(vs, 0, sizeof(*vs));
    return vs;
}

void V_freestate(V_State *Vs) {
    free(Vs->stack.nodes);

    for (int i = 0; i < Vs->instr.count; ++i) {
        V_Instr *ins = Vs->instr.instr + i;
        free(ins->ops);
    }
    free(Vs->instr.instr);

    free(Vs->func);

    for (int i = 0; i < Vs->api.count; ++i) {
        free((void*)Vs->api.api[i]);
    }

    free(Vs);
}

int V_load(V_State *Vs, FILE *f) {
    printf("XVM\nWritten by Outsky\n\nLoading...\n\n");

    // header
    char tmp[4];
    fread(tmp, 1, 4, f);
    if (strncmp(A_ID, tmp, 4) != 0) {
        printf("file format not support\n");
        return 0;
    }

    fread(&Vs->major, 1, 1, f);
    fread(&Vs->minor, 1, 1, f);

    fread(&Vs->stack.size, 4, 1, f);
    Vs->stack.nodes = (V_Value*)malloc(sizeof(V_Value) * Vs->stack.size);
    Vs->stack.top = 0;
    Vs->stack.frame = 0;

    fread(&Vs->global, 4, 1, f);
    int n = 0;
    fread(&n, 1, 1, f);
    fread(&Vs->mainidx, 4, 1, f);
    if (n != 1) {
        Vs->mainidx = -1;
    }

    // instruction
    Vs->instr.ip = 0;
    fread(&Vs->instr.count, 4, 1, f);
    Vs->instr.instr = (V_Instr*)malloc(sizeof(V_Instr) * Vs->instr.count);
    memset(Vs->instr.instr, 0, sizeof(V_Instr) * Vs->instr.count);
    for (int i = 0; i < Vs->instr.count; ++i) {
        V_Instr *ins = Vs->instr.instr + i;
        fread(&ins->opcode, 2, 1, f);
        fread(&ins->opcount, 1, 1, f);
        ins->ops = (V_Value*)malloc(sizeof(V_Value) * ins->opcount);
        memset(ins->ops, 0, sizeof(V_Value) * ins->opcount);
        for (int j = 0; j < ins->opcount; ++j) {
            V_Value *v = ins->ops + j;
            fread(&v->type, 1, 1, f);
            if (v->type == A_OT_FLOAT) {
                fread(&v->u.f, sizeof(v->u.f), 1, f);
            } else {
                fread(&v->u.n, sizeof(v->u.n), 1, f);
            }
            if(v->type == A_OT_ABS_SIDX || v->type == A_OT_REL_SIDX) {
                fread(&v->idx, sizeof(v->idx), 1, f);
            }
        }
    }

    // string
    n = 0;
    fread(&n, 4, 1, f);
    int sl = 0;
    char *tmpstr = NULL;
    for (int i = 0; i < n; ++i) {
        sl = 0;
        fread(&sl, 4, 1, f);
        tmpstr = (char*)malloc(sizeof(char) * (sl + 1));
        fread(tmpstr, sizeof(char), sl, f);
        tmpstr[sl] = '\0';
        for (int j = 0; j < Vs->instr.count; ++j) {
            V_Instr *ins = Vs->instr.instr + j;
            for (int k = 0; k < ins->opcount; ++k) {
                V_Value *v = ins->ops + k;
                if (v->type == A_OT_STRING && v->u.n == i) {
                    v->u.s = strdup(tmpstr);
                }
            }
        }
        free(tmpstr);
    }

    // function
    n = 0;
    fread(&n, 4, 1, f);
    Vs->func = (V_Func*)malloc(sizeof(V_Func) * n);
    memset(Vs->func, 0, sizeof(V_Func) * n);
    for (int i = 0; i < n; ++i) {
        V_Func *fn = Vs->func + i;
        fread(&fn->entry, 4, 1, f);
        fread(&fn->param, 1, 1, f);
        fread(&fn->local, 4, 1, f);
        fn->stack = 0;
    }

    // api
    fread(&Vs->api.count, 4, 1, f);
    Vs->api.api = (char**)malloc(sizeof(char*) * Vs->api.count);
    for (int i = 0; i < Vs->api.count; ++i) {
        fread(&n, 1, 1, f);
        Vs->api.api[i] = (char*)malloc(sizeof(char) * (n + 1));
        fread(Vs->api.api[i], sizeof(char), n, f);
        Vs->api.api[i][n] = '\0';
    }

    if (ferror(f)) {
        printf("load finished with ferror\n");
        return 0;
    }

    printf("load successfully!\n\n");
    printf("Load size: %ld\n", ftell(f));
    printf("XtremeScript VM Version %d.%d\n", Vs->major, Vs->minor);
    printf("Stack Size: %d\n", Vs->stack.size);
    printf("Instructions Assembled: %d\n", Vs->instr.count);
    printf("Globals: %d\n", Vs->global);
    printf("Host API Calls: %d\n", Vs->api.count);
    printf("_Main() Present: %s", Vs->mainidx >= 0 ? "YES" : "No");
    if (Vs->mainidx >= 0) {
        printf(" (Index %d)", Vs->mainidx);
    }

    return 1;
}

void V_run(V_State *Vs) {
    printf("\n\nrunning...\n\n");
    if (Vs->mainidx < 0) {
        printf("No _Main(), nothing to run\n");
        return;
    }

    Vs->instr.ip = Vs->func[Vs->mainidx].entry;
    for (;;) {
        if (Vs->instr.ip >= Vs->instr.count) {
            break;
        }

        V_Instr *ins = Vs->instr.instr + Vs->instr.ip;
        printf("%d> %s %d\n", Vs->instr.ip, _opnames[ins->opcode], ins->opcount);

        ++Vs->instr.ip;
    }

    printf("\nrun successfully!\n");
}
