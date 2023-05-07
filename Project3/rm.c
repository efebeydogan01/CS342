#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "rm.h"

typedef struct threadInfo {
    pthread_t realID;
    int selectedID;
    int active;
} threadInfo;

// global variables

int DA;  // indicates if deadlocks will be avoided or not
int N;   // number of processes
int M;   // number of resource types
int ExistingRes[MAXR]; // Existing resources vector

//..... other definitions/variables .....
threadInfo threadInf[MAXP];

int maxDemand[MAXP][MAXR];
int available[MAXR];
int allocation[MAXP][MAXR];
int need[MAXP][MAXR];
int Request[MAXP][MAXR];

pthread_mutex_t lock;
pthread_cond_t cv[MAXP];
// end of global variables

int getSelectedID(pthread_t threadID) {
    int res = -1;
    for (int i = 0; i < N; i++) {
        if (pthread_equal(threadID, threadInf[i].realID)) {
            res = threadInf[i].selectedID;
            break;
        }
    }
    return res;
}

int rm_thread_started(int tid)
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    if (tid < 0 || tid >= N) {
        printf("invalid thread id supplied to rm_thread_started");
        pthread_mutex_unlock(&lock);
        return -1;
    }
    threadInf[tid].selectedID = tid;
    threadInf[tid].realID = pthread_self();
    threadInf[tid].active = 1;
    pthread_mutex_unlock(&lock);
    return (ret);
}


int rm_thread_ended()
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    int tid = getSelectedID(pthread_self());
    if (tid == -1) {
        printf("thread information cannot be found in rm_thread_ended");
        pthread_mutex_unlock(&lock);
        return -1;
    }
    threadInf[tid].active = 0;
    pthread_mutex_unlock(&lock);
    return (ret);
}


int rm_claim (int claim[])
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    int tid = getSelectedID(pthread_self());
    if (tid == -1) {
        printf("thread information cannot be found in rm_claim");
        pthread_mutex_unlock(&lock);
        return -1;
    }
    
    if (DA == 1) {
        for (int i = 0; i < M; i++) {
            if (claim[i] > ExistingRes[i]) {
                printf("process claims more resources than existing");
                pthread_mutex_unlock(&lock);
                return -1;
            }
            maxDemand[tid][i] = claim[i];
            need[tid][i] = maxDemand[tid][i] - allocation[tid][i];
        }
    }
    pthread_mutex_unlock(&lock);
    return(ret);
}


int rm_init(int p_count, int r_count, int r_exist[],  int avoid)
{
    int i;
    int ret = 0;
    
    DA = avoid;
    N = p_count;
    M = r_count;
    // do checks
    if (N > MAXP || M > MAXR)
        return -1;
    if (DA != 1 && DA != 0)
        return -1;
    // initialize (create) resources
    for (i = 0; i < M; ++i){
        if (r_exist[i] < 0)
            return -1;
        ExistingRes[i] = r_exist[i];
        available[i] = r_exist[i];
    }
    // resources initialized (created)
    
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            allocation[i][j] = 0;
            need[i][j] = 0;
            Request[i][j] = 0;
        }
    }

    if (pthread_mutex_init(&lock, NULL) != 0 ) {
        printf("mutex lock cannot be initialized");
        return -1;
    }
    for (int i = 0; i < N; i++) {
        if (pthread_cond_init(&cv[i], NULL) != 0) {
            printf("condition variable cannot be initialized");
            return -1;
        }
    }

    return  (ret);
}

int canAllocate(int request[]) {
    for (int i = 0; i < M; i++) {
        if (request[i] > ExistingRes[i])
            return 0;
    }
    return 1;
}

int isAvailable(int request[]) {
    for (int i = 0; i < M; i++) {
        if (request[i] > available[i])
            return 0;
    }
    return 1;
}

void allocate(int tid, int request[]) {
    for (int i = 0; i < M; i++) {
        available[i] -= request[i];
        allocation[tid][i] += request[i];
        need[tid][i] = maxDemand[tid][i] - allocation[tid][i];
    }
}

int rm_request (int request[])
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    int tid = getSelectedID(pthread_self());
    if (tid == -1) {
        printf("thread information cannot be found in rm_request");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    if (!canAllocate(request)) {
        printf("more resources requested than available");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    // maintain request matrix
    for (int i = 0; i < M; i++) {
        Request[tid][i] = request[i];
    }

    if (DA == 0) { // deadlock detection
        while (!isAvailable(request)) // wait until resources become available
            pthread_cond_wait(&cv[tid], &lock);
        
    }
    else { // deadlock avoidance

    }

    pthread_mutex_unlock(&lock);
    return(ret);
}


int rm_release (int release[])
{
    int ret = 0;

    return (ret);
}


int rm_detection()
{
    int ret = 0;
    
    return (ret);
}


void rm_print_state (char hmsg[])
{
    return;
}
