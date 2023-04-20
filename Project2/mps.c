#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <limits.h>
#include "list.c"
#define M 0
#define S 1
#define RM 0
#define LM 1
#define NA -1
#define RR 0
#define FCFS 1
#define SJF 2

typedef struct Parameters {
    int N;
    int SAP;  // M:0, S:1
    int QS;   // RM:0, LM:1, NA:-1 
    int ALG;  // RR:0, FCFS:1, SJF:2
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
    int pid = (long) param;
    BurstItem *burstItem;

    while (1) {
        int qid = (parameters.SAP == M) ? pid : 0;

        pthread_mutex_lock(&lock[qid]);
        // SJF
        if (parameters.ALG == SJF) {
            burstItem = dequeueShortest(&queues[qid]);
        }
        // RR or FCFS
        else {
            burstItem = dequeue(&queues[qid]);
        }
        pthread_mutex_unlock(&lock[qid]);

        if (!burstItem)
            usleep(1000);
        // compute the burst duration to sleep
        else {
            int burstDuration = burstItem->remainingTime;
            if (parameters.ALG == RR)
                burstDuration = min(burstDuration, parameters.Q);
            usleep(burstDuration * 1000);
        }

    }
}

void selectQueue(BurstItem *item) {
    // single queue
    if (parameters.SAP == S) {
        pthread_mutex_lock(&lock[0]);
        item->processorID = 0;
        enqueue(queues[0], item);
        pthread_mutex_unlock(&lock[0]);
    }
    // multi queue - round robin method
    else if (parameters.QS == RM) {
        int qid = (item->pid - 1) % parameters.N;
        pthread_mutex_lock(&lock[qid]);
        item->processorID = qid;
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

        item->processorID = qid;
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
        .SAP = 0,
        .QS = 0,
        .ALG = 0,
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

    int queueCount = (parameters.SAP == M) ? parameters.N : 1;
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