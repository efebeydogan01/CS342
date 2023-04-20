#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#define DUMMY_ID -31

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

void print_list(list *lst) {
    node_t * current = lst->head;

    while (current != NULL) {
        printf("%d\n", current->item->pid);
        current = current->next;
    }
}

int isEmpty(list *lst) {
    if (!lst || lst->size == 0) {
        return 1;
    }
    return 0;
}

int isDummy(BurstItem *item) {
    if (item && item->pid == DUMMY_ID) {
        return 1;
    }
    return 0;
}

void enqueue(list *lst, BurstItem *item) {
    if (!lst)
        lst = createList();
    
    if (!lst->head) {
        node_t *head = (node_t *) malloc(sizeof(node_t));
        head->item = item;
        head->next = NULL;
        lst->head = head;
        lst->tail = head;
    }
    else {
        node_t *tail = lst->tail;

        tail->next = (node_t *) malloc(sizeof(node_t));
        tail->next->item = item;
        tail->next->next = NULL;
        lst->tail = tail->next;
    }
    lst->size++;
}

BurstItem* dequeue(list *lst) {
    if (isEmpty(lst))
        return NULL;

    BurstItem *retval = NULL;
    node_t *next_node = NULL;
    node_t *head = lst->head;

    next_node = head->next;
    retval = head->item;
    lst->head = next_node;
    lst->size--;
    free(head);

    return retval;
}

BurstItem* dequeueShortest(list *lst) {
    if (isEmpty(lst))
        return NULL;

    node_t *cur = lst->head;
    node_t *dq = cur;
    node_t *dq_prev = NULL;
    node_t *cur_prev = NULL;
    int curTime = cur->item->burstLength;

    while (cur) {
        // item not dummy and is a better candidate for the shortest 
        if (cur->item->pid != DUMMY_ID && cur->item->remainingTime < curTime) {
            dq_prev = cur_prev;
            dq = cur;
        }
        cur_prev = cur;
        cur = cur->next;
    }

    if (!dq_prev) {
        return dequeue(lst);
    }
    dq_prev->next = dq->next;
    BurstItem *retval = dq->item;
    lst->size--;
    free(dq);
    return retval;
}
