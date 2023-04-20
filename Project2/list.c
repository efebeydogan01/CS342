#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct BurstItem {
    int pid;
    int burstLength;
    int arrivalTime;
    int remainingTime;
    int finishTime;
    int turnaroundTime;
    int processorID;
} BurstItem;

typedef struct node {
    BurstItem *item;
    struct node *next;
} node_t;

typedef struct list {
    struct node *head;
    struct node *tail;
    int size;
} list;

list* createList() {
    list *lst = (list *) malloc(sizeof(list));
    lst->head = NULL;
    lst->tail = NULL;
    lst->size = 0;
    return lst;
}

void print_list(list * lst) {
    node_t * current = lst->head;

    while (current != NULL) {
        printf("%d\n", current->item->pid);
        current = current->next;
    }
}

void enqueue(list *lst, BurstItem *item) {
    if (!lst) {
        node_t *head = (node_t *) malloc(sizeof(node_t));
        head->item = item;
        head->next = NULL;
        lst->head = head;
        lst->tail = head;
        lst->size = 1;
    }
    else {
        node_t *tail = lst->tail;

        tail->next = (node_t *) malloc(sizeof(node_t));
        tail->next->item = item;
        tail->next->next = NULL;
        lst->tail = tail->next;
    }
}

BurstItem* dequeue(list ** lst) {
    BurstItem *retval = NULL;
    node_t * next_node = NULL;

    if (*lst == NULL) {
        return NULL;
    }
    node_t *head = (*lst)->head;
    next_node = head->next;
    retval = head->item;
    free(head);
    (*lst)->head = next_node;

    return retval;
}

