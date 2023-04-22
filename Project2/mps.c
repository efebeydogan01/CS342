#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
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
    int random;
    int T, T1, T2, L, L1, L2, PC;
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

long getTimestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    long seconds = current_time.tv_sec - parameters.start_time.tv_sec;
    long useconds = current_time.tv_usec - parameters.start_time.tv_usec;
    return ((seconds) * 1000 + useconds / 1000.0);
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
                printf("time=%ld, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", getTimestamp(), burstItem->processorID, burstItem->pid, burstItem->burstLength, burstItem->remainingTime);
            else if (parameters.outmode == 3) 
                printf("Burst with id %d is selected to run in CPU with id %d\n", burstItem->pid, pid);

            usleep(burstDuration * 1000);

            burstItem->remainingTime -= burstDuration;

            // re-enqueue for RR
            if (parameters.ALG == RR && burstItem->remainingTime > 0) {
                if (parameters.outmode == 3) 
                    printf("Burst with id %d is added to the end of the queue after time slice expired (round-robin scheduling, remaining time: %d)\n", burstItem->pid, burstItem->remainingTime);
                
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
                    printf("Burst with id %d has finished its burst. (finish time: %ld, turnaround time: %ld, waiting time: %ld)\n", burstItem->pid, burstItem->finishTime, 
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
            if (load < minLoad) {
                minLoad = load;
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
    printf("\n%-5s %-4s %-10s %-5s %-7s %-11s %-10s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
    for (int i = 0; i < size; i++) {
        item = bursts[i];
        tt_sum += item->turnaroundTime;
        printf("%-5d %2d %7d %8ld %7ld %7ld %11ld\n", 
            item->pid, item->processorID, item->burstLength, item->arrivalTime, item->finishTime, item->waitingTime, item->turnaroundTime);
    }

    printf("average turnaround time: %d\n", tt_sum / size);
}

int getRandomTime(int mean, int lower, int upper) {
    double u, x;

    while (1) {
        u = (rand() % RAND_MAX) / (double) RAND_MAX;
        x = -1 * log(1 - u) * mean;

        if (x >= lower && x <= upper)
            return x;
    }
    return -1;
}

BurstItem* generateBurstItem(int pid, int burstLength) {
    BurstItem *burstItem = (BurstItem *) malloc(sizeof(BurstItem));
    
    *burstItem = (BurstItem) {
        .pid = pid,
        .burstLength = burstLength,
        .arrivalTime = getTimestamp(),
        .remainingTime = burstLength,
        .finishTime = -1,
        .turnaroundTime = -1,
        .waitingTime = -1,
        .processorID = -1
    };
    return burstItem;
}

void readParameters(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        char *cur = argv[i];
        if (strcmp(cur, "-n") == 0) {
            i++;
            parameters.N = atoi(argv[i]);
        }
        else if (strcmp(cur, "-a") == 0) {
            i++;
            parameters.SAP = (strcmp(argv[i], "M") == 0) ? 0 : 1; 
            i++;
            if (strcmp(argv[i], "RM") == 0)
                parameters.QS = 0;
            else if (strcmp(argv[i], "LM") == 0)
                parameters.QS = 1;
            else
                parameters.QS = -1;
        }
        else if (strcmp(cur, "-s") == 0) {
            i++;
            if (strcmp(argv[i], "FCFS") == 0)
                parameters.ALG =  1;
            else if (strcmp(argv[i], "SJF") == 0)
                parameters.ALG = 2;
            else
                parameters.ALG = 0;
            i++;
            parameters.Q = atoi(argv[i]);
        }
        else if (strcmp(cur, "-i") == 0) {
            i++;
            parameters.infile = argv[i];
        }
        else if (strcmp(cur, "-m") == 0) {
            i++;
            parameters.outmode = atoi(argv[i]);
        }
        else if (strcmp(cur, "-o") == 0) {
            i++;
            parameters.outfile = argv[i];
        }
        else if (strcmp(cur, "-r") == 0) {
            parameters.random = 1;
            i++;
            parameters.T = atoi(argv[i]);
            i++;
            parameters.T1 = atoi(argv[i]);
            i++;
            parameters.T2 = atoi(argv[i]);
            i++;
            parameters.L = atoi(argv[i]);
            i++;
            parameters.L1 = atoi(argv[i]);
            i++;
            parameters.L2 = atoi(argv[i]);
            i++;
            parameters.PC = atoi(argv[i]);
        }
    }

    // if (!r && )
}

int main(int argc, char* argv[]) {
    // get the start time of the program
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    parameters = (Parameters) {
        .N = 2,
        .SAP = M,
        .QS = RM,
        .ALG = RR,
        .Q = 20,
        .random = 0,
        .T = 200, .T1 = 10, .T2 = 1000, .L = 100, .L1 = 10, .L2 = 500, .PC = 10,
        .infile = "in.txt",
        .outmode = 2,//1,
        .outfile = "",
        .start_time = start_time
    };

    readParameters(argc, argv);

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
    if (fp == NULL) {
        printf("Input file not found.\n");
        exit(EXIT_FAILURE);
    }
    
    int id = 1;

    if (parameters.random == 0) {
        // read from file
        while ((read = getline(&line, &len, fp)) != -1) {
            char *token = strtok(line, " ");
            if (strcmp(token, "PL") == 0) {
                // get the burst time
                token = strtok(NULL, " \n");
                BurstItem *burstItem = generateBurstItem(id, atoi(token));
                id++;
                selectQueue(burstItem);
            }
            else if (strcmp(token, "IAT") == 0) {
                // get the interarrival time for sleep
                token = strtok(NULL, " \n");
                usleep(atoi(token) * 1000);
            }
        }
        fclose(fp);
        if (line) free(line);
    }
    else {
        // generate random bursts and interarrival times
        BurstItem *burstItem;
        int burstLength, interarrivalTime;

        for (int id = 1; id <= parameters.PC; id++) {
            burstLength = getRandomTime(parameters.L, parameters.L1, parameters.L2);
            burstItem = generateBurstItem(id, burstLength);
            selectQueue(burstItem);

            if (id != parameters.PC) {
                interarrivalTime = getRandomTime(parameters.T, parameters.T1, parameters.T2);
                usleep(interarrivalTime * 1000);
            }
        }
    }

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

    // free all of the allocated memory
    for (int i = 0; i < queueCount; i++) {
        freeList(queues[i]);
    }
    free(queues);
    free(lock);
    
    for (int i = 0; i < finishedBursts->size; i++) {
        free(sortedBursts[i]);
    }
    free(sortedBursts);
    // freeList(finishedBursts);
}