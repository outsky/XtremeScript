#include <stdio.h>
#include <string.h>
#include "list.h"

int main() {
    list *l = list_new();

    lnode *n1 = list_newnode(strdup("abc"));
    lnode *n2 = list_newnode(strdup("def"));
    lnode *n3 = list_newnode(strdup("ghi"));

    list_pushback(l, n1);
    list_pushback(l, n2);
    list_pushback(l, n3);

    for (lnode *n = l->head; n != NULL; n = n->next) {
        printf("%s\n", (char*)n->data);
    }

    list_free(l);
    return 0;
}
