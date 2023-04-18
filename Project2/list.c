#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct BurstItem {
    int pid;
    int burstLenght;
    struct timeval arrivalTime;
    int remainingTime;
    int finishTime;
    int turnaroundTime;
    int processorID;
} BurstItem;

typedef struct node {
    BurstItem *item;
    struct node *next;
} node_t;

node_t* initializeList(BurstItem *item) {
    node_t *head = (node_t *) malloc(sizeof(node_t));
    head->item = item;
    head->next = NULL;
    return head;
}

void print_list(node_t * head) {
    node_t * current = head;

    while (current != NULL) {
        printf("%d\n", current->item->pid);
        current = current->next;
    }
}

void enqueue(node_t * head, BurstItem *item) {
    node_t * current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new variable */
    current->next = (node_t *) malloc(sizeof(node_t));
    current->next->item = item;
    current->next->next = NULL;
}

BurstItem* dequeue(node_t ** head) {
    BurstItem *retval = NULL;
    node_t * next_node = NULL;

    if (*head == NULL) {
        return NULL;
    }

    next_node = (*head)->next;
    retval = (*head)->item;
    free(*head);
    *head = next_node;

    return retval;
}

