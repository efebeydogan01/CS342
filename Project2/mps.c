#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include "list.c"

struct timeval start_time;
list **queues;
pthread_mutex_t *lock;
pthread_t *pid;

static void* schedule(void *param) {
}

int main(int argc, char* argv[]) {
    // get the start time of the program
    gettimeofday(&start_time, NULL);

    int N = 2;
    char *SAP = "M";
    char *QS = "RM";
    char *ALG = "RR";
    int Q = 20;
    char *infile = "in.txt";
    int outmode = 1;
    char *outfile = "out.txt";

    // BurstItem *item = (BurstItem *) malloc(sizeof(BurstItem));
    // item->pid = 31;
    // list *lst = initializeList(item);
    // enqueue(lst, item);
    // dequeue(&lst);
    // dequeue(&lst);
    // dequeue(&lst);
    // print_list(lst);

    int multiFlag = strcmp(SAP, "M") ? 0 : 1; // true if multiqueue approach is used
    int queueCount = multiFlag ? N : 1;
    queues = (list **) malloc(sizeof(list*) * queueCount);
    lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t*) * queueCount);
    pid = (pthread_t *) malloc(sizeof(pthread_t*) * N);

    for (int i = 0; i < queueCount; i++) {
        queues[i] = NULL;
    }

    // create N threads to simulate N processors
    for (int i = 0; i < N; i++) {
        int err = pthread_create(&pid[i], NULL, schedule, i);
        if (err) {
            printf("error occured: %d\n",  err);
            exit(1);
        }
    }

    // read the input file line by line
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    size_t read;

    fp = fopen(infile, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    
    while ((read = getline(&line, &len, fp)) != -1) {
        // printf("Retrieved line of length %zu:\n", read);
        // printf("%s", line);
        char *token = strtok(line, " ");
        if (strcmp(token, "PL") == 0) {
            // get the burst time
            token = strtok(NULL, " ");
        }
    }

    fclose(fp);
    if (line)
        free(line);
}