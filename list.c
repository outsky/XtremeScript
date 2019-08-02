#include <stdlib.h>
#include "list.h"

lnode* list_newnode(void *data) {
    lnode *n = (lnode*)malloc(sizeof(*n));
    n->data = data;
    n->next = NULL;
    return n;
}

list* list_new() {
    list *l = (list*)malloc(sizeof(*l));
    l->head = l->tail = NULL;
    l->count = 0;
    return l;
}

void list_free(list *l) {
    lnode *n = l->head;
    for (;;) {
        if (n == NULL) {
            break;
        }

        lnode *tmp = n;
        n = n->next;

        free(tmp->data);
        free(tmp);
    }
    free(l);
}

void list_pushback(list *l, lnode *n) {
    n->next = NULL;
    if (l->count == 0) {
        l->head = l->tail = n;
    } else {
        l->tail->next = n;
        l->tail = n;
    }
    ++l->count;
}
