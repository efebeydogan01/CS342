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
    int inFlag;
    int outmode;
    char *outfile;
    int outFlag;
    struct timeval start_time;
} Parameters;

Parameters parameters;
list **queues;
pthread_mutex_t *lock;
list *finishedBursts;
pthread_mutex_t finishedLock;
pthread_mutex_t outfileLock;
FILE *optr;

long getTimestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    long seconds = current_time.tv_sec - parameters.start_time.tv_sec;
    long useconds = current_time.tv_usec - parameters.start_time.tv_usec;
    return ((seconds) * 1000 + useconds / 1000.0);
}

static void* schedule(void *param) {
    int pid = (long) param; // id of the cpu
    int qid = (parameters.SAP == M) ? (pid-1) : 0;
    BurstItem *burstItem;

    while (1) {
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
            else {
                free(burstItem);
            }
            if (parameters.outmode == 3) {
                if (parameters.outFlag) {
                    pthread_mutex_lock(&outfileLock);
                    fprintf(optr, "t=%-5ld\tcpu=%-2d finished working\n", getTimestamp(), pid);
                    pthread_mutex_unlock(&outfileLock);
                }
                else {
                    printf("t=%-5ld\tcpu=%-2d finished working\n", getTimestamp(), pid);
                }
            }
            pthread_mutex_unlock(&lock[qid]);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&lock[qid]);

        if (!burstItem) {
            usleep(1000);
        }
        // compute the burst duration to sleep
        else {
            if (parameters.ALG == RR && parameters.SAP == S)
                burstItem->processorID = pid;
            int burstDuration = burstItem->remainingTime;
            if (parameters.ALG == RR && parameters.Q < burstDuration)
                burstDuration = parameters.Q;

            if (parameters.outmode == 2) {
                if (parameters.outFlag) {
                    pthread_mutex_lock(&outfileLock);
                    fprintf(optr, "time=%ld, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", getTimestamp(), burstItem->processorID, burstItem->pid, burstItem->burstLength, burstItem->remainingTime);
                    pthread_mutex_unlock(&outfileLock);
                }
                else 
                    printf("time=%ld, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", getTimestamp(), burstItem->processorID, burstItem->pid, burstItem->burstLength, burstItem->remainingTime);
            }
            else if (parameters.outmode == 3) {
                if (parameters.outFlag) {
                    pthread_mutex_lock(&outfileLock);
                    fprintf(optr, "t=%-5ld\tpid=%-2d--selected--> cpu=%-2d \t\t (burstlen=%d, remainingtime=%d)\n", getTimestamp(), burstItem->pid, pid, burstItem->burstLength, burstItem->remainingTime);
                    pthread_mutex_unlock(&outfileLock);
                }
                else 
                    printf("t=%-5ld\tpid=%-2d--selected--> cpu=%-2d \t\t (burstlen=%d, remainingtime=%d)\n", getTimestamp(), burstItem->pid, pid, burstItem->burstLength, burstItem->remainingTime);
            }

            // simulate cpu burst (by sleeping)
            usleep(burstDuration * 1000);

            burstItem->remainingTime -= burstDuration;

            // re-enqueue for RR
            if (parameters.ALG == RR && burstItem->remainingTime > 0) {
                if (parameters.outmode == 3) {
                    if (parameters.outFlag) {
                        pthread_mutex_lock(&outfileLock);
                        fprintf(optr, "t=%-5ld\tpid=%-2d--time slice expired-- cpu=%-2d \t (burstlen=%d, remainingtime=%d, getting re-enqueued)\n", getTimestamp(), burstItem->pid, pid, burstItem->burstLength, burstItem->remainingTime);
                        pthread_mutex_unlock(&outfileLock);
                    }
                    else
                        printf("t=%-5ld\tpid=%-2d--time slice expired-- cpu=%-2d \t (burstlen=%d, remainingtime=%d, getting re-enqueued)\n", getTimestamp(), burstItem->pid, pid, burstItem->burstLength, burstItem->remainingTime);
                }

                pthread_mutex_lock(&lock[qid]);
                enqueue(queues[qid], burstItem);
                pthread_mutex_unlock(&lock[qid]);

            }
            // compute times and add item to finished bursts
            else if (burstItem->remainingTime == 0) {
                burstItem->finishTime = getTimestamp();
                burstItem->turnaroundTime = burstItem->finishTime - burstItem->arrivalTime;
                burstItem->waitingTime = burstItem->turnaroundTime - burstItem->burstLength;

                if (parameters.outmode == 3) {
                    if (parameters.outFlag) {
                        pthread_mutex_lock(&outfileLock);
                        fprintf(optr, "t=%-5ld\tpid=%-2d--finished-- cpu=%-2d \t\t (finish time: %ld, turnaround time: %ld, waiting time: %ld)\n", getTimestamp(), burstItem->pid, pid, burstItem->finishTime, 
                        burstItem->turnaroundTime, burstItem->waitingTime);
                        pthread_mutex_unlock(&outfileLock);
                    }
                    else
                        printf("t=%-5ld\tpid=%-2d--finished-- cpu=%-2d \t\t (finish time: %ld, turnaround time: %ld, waiting time: %ld)\n", getTimestamp(), burstItem->pid, pid, burstItem->finishTime, 
                        burstItem->turnaroundTime, burstItem->waitingTime);
                }

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
        item->processorID = 1;
        enqueue(queues[0], item);
        pthread_mutex_unlock(&lock[0]);

        if (parameters.outmode == 3) {
            if (parameters.outFlag) {
                pthread_mutex_lock(&outfileLock);
                fprintf(optr, "t=%-5ld\tpid=%-2d--enqueued--> common queue\t (burstlen=%d)\n", getTimestamp(), item->pid, item->burstLength);
                pthread_mutex_unlock(&outfileLock);
            }
            else
                printf("t=%-5ld\tpid=%-2d--enqueued--> common queue\t (burstlen=%d)\n", getTimestamp(), item->pid, item->burstLength);
        }
    }
    // multi queue - round robin method
    else if (parameters.QS == RM) {
        int qid = (item->pid - 1) % parameters.N;
        pthread_mutex_lock(&lock[qid]);
        item->processorID = qid + 1;
        enqueue(queues[qid], item);
        if (parameters.outmode == 3) {
            if (parameters.outFlag) {
                pthread_mutex_lock(&outfileLock);
                fprintf(optr, "t=%-5ld\tpid=%-2d--enqueued--> queue for cid=%d\t (burstlen=%d)\n", getTimestamp(), item->pid, item->processorID, item->burstLength);
                pthread_mutex_unlock(&outfileLock);
            }
            else
                printf("t=%-5ld\tpid=%-2d--enqueued--> queue for cid=%d\t (burstlen=%d)\n", getTimestamp(), item->pid, item->processorID, item->burstLength);
        }
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
            if (load < minLoad) {
                minLoad = load;
                qid = i;
            }
        }

        item->processorID = qid + 1;
        enqueue(queues[qid], item);
        if (parameters.outmode == 3)  {
            if (parameters.outFlag) {
                pthread_mutex_lock(&outfileLock);
                fprintf(optr, "t=%-5ld\tpid=%-2d--enqueued--> queue for cid=%d\t (burstlen=%d)\n", getTimestamp(), item->pid, item->processorID, item->burstLength);
                pthread_mutex_unlock(&outfileLock);
            }
            else
                printf("t=%-5ld\tpid=%-2d--enqueued--> queue for cid=%d\t (burstlen=%d)\n", getTimestamp(), item->pid, item->processorID, item->burstLength);  
        }      
        // release all locks
        for (int i = 0; i < parameters.N; i++) {
            pthread_mutex_unlock(&lock[i]);
        }
    }
}

void addDummyItem() {
    // single queue
    if (parameters.SAP == S) {
        pthread_mutex_lock(&lock[0]);
        enqueue(queues[0], createDummyItem());
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
    if (parameters.outFlag) {
        pthread_mutex_lock(&outfileLock);
        fprintf(optr, "\n%-5s %-4s %-10s %-5s %-7s %-11s %-10s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
        for (int i = 0; i < size; i++) {
            item = bursts[i];
            tt_sum += item->turnaroundTime;
            fprintf(optr, "%-5d %2d %7d %8ld %7ld %7ld %11ld\n", 
                item->pid, item->processorID, item->burstLength, item->arrivalTime, item->finishTime, item->waitingTime, item->turnaroundTime);
        }
        fprintf(optr, "average turnaround time: %d", tt_sum / size);
        pthread_mutex_unlock(&outfileLock);
    }
    else {
        printf("\n%-5s %-4s %-10s %-5s %-7s %-11s %-10s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
        for (int i = 0; i < size; i++) {
            item = bursts[i];
            tt_sum += item->turnaroundTime;
            printf("%-5d %2d %7d %8ld %7ld %7ld %11ld\n", 
                item->pid, item->processorID, item->burstLength, item->arrivalTime, item->finishTime, item->waitingTime, item->turnaroundTime);
        }
        printf("average turnaround time: %d\n", tt_sum / size);
    }
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
            parameters.inFlag = 1;
            parameters.infile = argv[i];
        }
        else if (strcmp(cur, "-m") == 0) {
            i++;
            parameters.outmode = atoi(argv[i]);
        }
        else if (strcmp(cur, "-o") == 0) {
            i++;
            parameters.outfile = argv[i];
            parameters.outFlag = 1;
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

    // condition for when neither i nor r is supplied
    if (parameters.random == 0 && parameters.inFlag == 0)
        parameters.random = 1;
}

int main(int argc, char* argv[]) {
    // get the start time of the program
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    srand(time(NULL));

    parameters = (Parameters) {
        .N = 2,
        .SAP = M,
        .QS = RM,
        .ALG = RR,
        .Q = 20,
        .random = 0,
        .T = 200, .T1 = 10, .T2 = 1000, .L = 100, .L1 = 10, .L2 = 500, .PC = 10,
        .infile = "",
        .inFlag = 0,
        .outmode = 1,
        .outfile = "",
        .start_time = start_time,
        .outFlag = 0
    };

    readParameters(argc, argv);

    int queueCount = (parameters.SAP == M) ? parameters.N : 1;
    queues = (list **) malloc(sizeof(list*) * queueCount);
    lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t) * queueCount);

    if (pthread_mutex_init(&finishedLock, NULL)) {
        printf("couldn't initialize finished lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&outfileLock, NULL)) {
        printf("couldn't initialize outfile lock");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < queueCount; i++) {
        int mut = pthread_mutex_init(&lock[i], NULL);
        if (mut) {
            printf("couldn't initialize queue lock");
            exit(EXIT_FAILURE);
        }
    }

    // open the output file if flag is given
    if (parameters.outFlag) {
        optr = fopen(parameters.outfile, "w");
        if (!optr) {
            printf("cannot open output file");
            exit(1);
        }
    }

    if (parameters.outmode == 3) {
        if (parameters.outFlag) {
            pthread_mutex_lock(&outfileLock);
            fprintf(optr, "Number of processors in simulation: %d\n", parameters.N);
            if (parameters.SAP == 1)
                fprintf(optr, "single-queue approach\n");
            else if (parameters.SAP == 0 && parameters.QS == 0)
                fprintf(optr, "multi-queue approach with round robin method\n");
            else
                fprintf(optr, "multi-queue approach with load-balancing method\n");
            
            if (parameters.ALG == 0)
                fprintf(optr, "Scheduling algorithm: RR with time quantum %d ms\n", parameters.Q);
            else if (parameters.ALG == 1)
                fprintf(optr, "Scheduling algorithm: FCFS\n");
            else
                fprintf(optr, "Scheduling algorithm: SJF\n");
            
            if (parameters.random == 1) {
                fprintf(optr, "Bursts and interarrival times will be created randomly with parameters:\n");
                fprintf(optr, "T=%d, T1=%d, T2=%d, L=%d, L1=%d, L2=%d, PC=%d\n", parameters.T, parameters.T1, parameters.T2, parameters.L, parameters.L1, parameters.L2, parameters.PC);
            }
            else
                fprintf(optr, "Bursts and interarrival times will be read from an input file with name: %s\n", parameters.infile);
            pthread_mutex_unlock(&outfileLock);
        }
        else {
            printf("Number of processors in simulation: %d\n", parameters.N);
            if (parameters.SAP == 1)
                printf("single-queue approach\n");
            else if (parameters.SAP == 0 && parameters.QS == 0)
                printf("multi-queue approach with round robin method\n");
            else
                printf("multi-queue approach with load-balancing method\n");
            
            if (parameters.ALG == 0)
                printf("Scheduling algorithm: RR with time quantum %d ms\n", parameters.Q);
            else if (parameters.ALG == 1)
                printf("Scheduling algorithm: FCFS\n");
            else
                printf("Scheduling algorithm: SJF\n");
            
            if (parameters.random == 1) {
                printf("Bursts and interarrival times will be created randomly with parameters:\n");
                printf("T=%d, T1=%d, T2=%d, L=%d, L1=%d, L2=%d, PC=%d\n", parameters.T, parameters.T1, parameters.T2, parameters.L, parameters.L1, parameters.L2, parameters.PC);
            }
            else
                printf("Bursts and interarrival times will be read from an input file with name: %s\n", parameters.infile);
        }
    }

    finishedBursts = (list *) malloc(sizeof(list));
    finishedBursts->head = NULL;
    finishedBursts->tail = NULL;
    finishedBursts->size = 0;
    pthread_t pid[parameters.N];

    for (int i = 0; i < queueCount; i++) {
        queues[i] = createList();
    }

    // create N threads to simulate N processors
    for (long i = 0; i < parameters.N; i++) {
        int err = pthread_create(&pid[i], NULL, schedule, (void *) (i+1));
        if (err) {
            printf("error occured: %d\n",  err);
            exit(1);
        }
    }
    
    int id = 1;

    if (parameters.random == 0) {
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
    
    // for (int i = 0; i < finishedBursts->size; i++) {
    //     free(sortedBursts[i]);
    // }
    free(sortedBursts);
    freeList(finishedBursts);

    // close output file
    if (parameters.outFlag) 
        fclose(optr);
}