#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

struct node {
   int value;
   struct node* next;
   struct node* prev;
};

struct node* createNode(int value) {
   struct node* newNode = (struct node*) malloc(sizeof( struct node));
   newNode->value = value;
   newNode->next = NULL;
   newNode->prev = NULL;
   return newNode;
}

void insert(struct node** head, int value) {
   if ( *head == NULL) {
      *head = createNode( value);
      return;
   }
   struct node* newNode = createNode(value);
   if ( (*head)->value >= value) {
      newNode->next = *head;
      newNode->next->prev = newNode;
      *head = newNode;
   }
   else {
      struct node* cur = *head;

      while (cur->next && cur->next->value < newNode->value) {
         cur = cur->next;
      }

      newNode->next = cur->next;
      if (cur->next) {
         newNode->next->prev = newNode;
      }
      cur->next = newNode;
      newNode->prev = cur;
   }
}

// function to test if the sorted doubly linked list is implemented correctly
void printList(struct node* head) {
   int count = 0;
   while (head) {
      printf("%d\n", head->value);
      head = head->next;
      count++;
   }
   printf("%d", count);
}

int main() {
   struct node* head = NULL;
   struct timeval start;
   struct timeval end;
   gettimeofday(&start, 0);

   // inserting 10000 random integers into the doubly linked list
   for ( int i = 0; i < 10000; i++) {
      insert(&head, rand());
   }

   gettimeofday(&end, 0);
   printf("It took %f seconds to insert 10000 elements into the list.\n", (end.tv_usec - start.tv_usec) / 1000000.0);
   printList(head);
   return 0;
}