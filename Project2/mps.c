#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <limits.h>
#include "list.c"

typedef struct Parameters {
    int N;
    char *SAP;
    char *QS;
    char *ALG;
    int Q;
    char *infile;
    int outmode;
    char *outfile;
    struct timeval start_time;
} Parameters;

Parameters parameters;
list **queues;
pthread_mutex_t *lock;


static void* schedule(void *param) {
    printf("%lu\n", (long) param);
}

void selectQueue(BurstItem *item) {
    // single queue
    if (strcmp(parameters.SAP, "S" == 0)) {
        pthread_mutex_lock(&lock[0]);
        enqueue(queues[0], item);
        pthread_mutex_unlock(&lock[0]);
    }
    // multi queue - round robin method
    else if (strcmp(parameters.QS, "RM" == 0)) {
        int qid = (item->pid - 1) % parameters.N;
        pthread_mutex_lock(&lock[qid]);
        enqueue(queues[qid], item);
        pthread_mutex_unlock(&lock[qid]);
    }
    // multi queue - load balancing method
    else {
        int minLoad = INT_MAX;
        int qid = -1;

        // get all locks
        for (int i = 0; i < parameters.N; i++) {
            pthread_mutex_lock(&lock[i]);
        }

        // find the queue with the least load
        for (int i = 0; i < parameters.N; i++) {
            int load = queues[i]->size;
            if (queues[i]->size < minLoad) {
                minLoad = queues[i]->size;
                qid = i;
            }
        } 

        enqueue(queues[qid], item);

        // release all locks
        for (int i = 0; i < parameters.N; i++) {
            pthread_mutex_lock(&lock[i]);
        }
    }
}

int main(int argc, char* argv[]) {
    // get the start time of the program
    gettimeofday(&parameters.start_time, NULL);

    parameters = (Parameters) {
        .N = 2,
        .SAP = "M",
        .QS = "RM",
        .ALG = "RR",
        .Q = 20,
        .infile = "in.txt",
        .outmode = 1,
        .outfile = "out.txt"
    };

    // BurstItem *item = (BurstItem *) malloc(sizeof(BurstItem));
    // item->pid = 31;
    // list *lst = initializeList(item);
    // enqueue(lst, item);
    // dequeue(&lst);
    // dequeue(&lst);
    // dequeue(&lst);
    // print_list(lst);

    int multiFlag = strcmp(parameters.SAP, "M") ? 0 : 1; // true if multiqueue approach is used
    int queueCount = multiFlag ? parameters.N : 1;
    queues = (list **) malloc(sizeof(list*) * queueCount);
    lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t) * queueCount);
    pthread_t pid[parameters.N];

    for (int i = 0; i < queueCount; i++) {
        queues[i] = NULL;
    }

    // create N threads to simulate N processors
    for (long i = 0; i < parameters.N; i++) {
        int err = pthread_create(&pid[i], NULL, schedule, (void *) i);
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

    fp = fopen(parameters.infile, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    
    int id = 1;
    while ((read = getline(&line, &len, fp)) != -1) {
        char *token = strtok(line, " ");
        if (strcmp(token, "PL") == 0) {
            // get the burst time
            token = strtok(NULL, " \n");
            //printf("burst time: %s\n", token);
            BurstItem *burstItem = (BurstItem *) malloc(sizeof(BurstItem));
            burstItem->pid = id;
            burstItem->burstLength = atoi(token);

            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            burstItem->arrivalTime = (current_time.tv_usec - parameters.start_time.tv_usec) / 1000;

            burstItem->remainingTime = atoi(token);

            burstItem->finishTime = -1;
            burstItem->turnaroundTime = -1;
            burstItem->processorID = -1;

            id++;
            selectQueue(burstItem);
        }
        else if (strcmp(token, "IAT") == 0) {
            token = strtok(NULL, " \n");
            int sleepTime = atoi(token) * 1000;
            usleep(sleepTime);
        }
    }

    fclose(fp);
    if (line)
        free(line);
}