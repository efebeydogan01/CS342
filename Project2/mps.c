#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
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
list *finishedBursts;
pthread_mutex_t finishedLock;

int getTimestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    return (current_time.tv_usec - parameters.start_time.tv_usec) / 1000;
}

static void* schedule(void *param) {
    int pid = (long) param;
    BurstItem *burstItem;

    while (1) {
        int qid = (parameters.SAP == M) ? pid : 0;

        pthread_mutex_lock(&lock[qid]);
        // SJF
        if (parameters.ALG == SJF) {
            burstItem = dequeueShortest(queues[qid]);
        }
        // RR or FCFS
        else {
            burstItem = dequeue(queues[qid]);
        }

        if (isDummy(burstItem)) {
            // read the dummy item for other threads to terminate
            if (parameters.SAP == S && parameters.N > 1) {
                enqueue(queues[qid], burstItem);
            }
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&lock[qid]);

        if (!burstItem) {
            usleep(1000);
        }
        // compute the burst duration to sleep
        else {
            int burstDuration = burstItem->remainingTime;
            if (parameters.ALG == RR && parameters.Q < burstDuration)
                burstDuration = parameters.Q;

            if (parameters.outmode == 2)
                printf("time=%d, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", getTimestamp(), burstItem->processorID, burstItem->pid, burstItem->burstLength, burstItem->remainingTime);
            else if (parameters.outmode == 3) 
                printf("Burst with id %d is selected to run in CPU with id %d\n", burstItem->pid, pid);

            usleep(burstDuration * 1000);

            burstItem->remainingTime -= burstDuration;

            // re-enqueue for RR
            if (parameters.ALG == RR && burstItem->remainingTime > 0) {
                if (parameters.outmode == 3) 
                    printf("Burst with id %d is added to the end of the queue after time sliced expired (round-robin scheduling, remaining time: %d)\n", burstItem->pid, burstItem->remainingTime);
                
                pthread_mutex_lock(&lock[qid]);
                enqueue(queues[qid], burstItem);
                pthread_mutex_unlock(&lock[qid]);

                if (parameters.outmode == 3)
                    printf("Burst with id %d is added to the end of the queue (round-robin scheduling)\n", burstItem->pid);  
            }
            // compute times and add item to finished bursts
            else if (burstItem->remainingTime == 0) {
                burstItem->finishTime = getTimestamp();
                burstItem->turnaroundTime = burstItem->finishTime - burstItem->arrivalTime;
                burstItem->waitingTime = burstItem->turnaroundTime - burstItem->burstLength;

                if (parameters.outmode == 3)
                    printf("Burst with id %d has finished its burst. (finish time: %d, turnaround time: %d, waiting time: %d)\n", burstItem->pid, burstItem->finishTime, 
                        burstItem->turnaroundTime, burstItem->waitingTime);

                pthread_mutex_lock(&finishedLock);
                enqueue(finishedBursts, burstItem);
                pthread_mutex_unlock(&finishedLock);
            }
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

        if (parameters.outmode == 3)
            printf("Burst with id %d and burst length %d is added to the end of the common queue (single-queue approach)\n", item->pid, item->burstLength);
    }
    // multi queue - round robin method
    else if (parameters.QS == RM) {
        int qid = (item->pid - 1) % parameters.N;
        pthread_mutex_lock(&lock[qid]);
        item->processorID = qid;
        enqueue(queues[qid], item);
        pthread_mutex_unlock(&lock[qid]);

        if (parameters.outmode == 3)
            printf("Burst with id %d and burst length %d is added to the queue of the processor with id %d (multiqueue with round robin)\n", item->pid, item->burstLength, qid);
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
            pthread_mutex_unlock(&lock[i]);
        }

        if (parameters.outmode == 3) 
            printf("Burst with id %d and burst length %d is added to the queue of the processor with id %d (multiqueue with load balancing)\n", item->pid, item->burstLength, qid);
        
    }
}

void addDummyItem() {
    // single queue
    if (parameters.SAP == S) {
        pthread_mutex_lock(&lock[0]);
        BurstItem *item = (BurstItem *) malloc(sizeof(BurstItem));
        // dummy item
        item->pid = DUMMY_ID;
        enqueue(queues[0], item);
        pthread_mutex_unlock(&lock[0]);
    }
    else {
        // get all locks
        for (int i = 0; i < parameters.N; i++) {
            pthread_mutex_lock(&lock[i]);
        }
        
        for (int i = 0; i < parameters.N; i++) {
            enqueue(queues[i], createDummyItem());
        } 

        // release all locks
        for (int i = 0; i < parameters.N; i++) {
            pthread_mutex_unlock(&lock[i]);
        }
    }
}

void printBursts(BurstItem **bursts, int size) {
    BurstItem *item;
    int tt_sum = 0;

    // print the table
    printf("%-6s%-6s%-8s%-8s%-8s%-8s%-8s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
    for (int i = 0; i < size; i++) {
        item = bursts[i];
        tt_sum += item->turnaroundTime;
        printf("%-6d%-6d%-8d%-8d%-8d%-8d%-8d\n", 
            item->pid, item->processorID, item->burstLength, item->arrivalTime, item->finishTime, item->waitingTime, item->turnaroundTime);
    }

    printf("average turnaround time: %d\n", tt_sum / size);
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
        .outmode = 3,//1,
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
    finishedBursts = (list *) malloc(sizeof(list));
    pthread_t pid[parameters.N];

    for (int i = 0; i < queueCount; i++) {
        queues[i] = createList();
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
            burstItem->arrivalTime = getTimestamp();
            burstItem->remainingTime = atoi(token);
            burstItem->finishTime = -1;
            burstItem->turnaroundTime = -1;
            burstItem->waitingTime = -1;
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

    // add dummy items to the end of every queue
    addDummyItem();

    // wait for threads to terminate
    for (int i = 0; i < parameters.N; i++) {
        if ( pthread_join( pid[i], NULL)) {
            printf("Error joining threads!");
            exit(1);
        }
    }

    BurstItem** sortedBursts = sort(finishedBursts);
    printBursts(sortedBursts, finishedBursts->size);
}