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
pthread_cond_t cv;
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

int isArraySmaller(int arr1[], int arr2[], int size) {
    for (int i = 0; i < size; i++)
        if (arr1[i] > arr2[i])
            return 0;
    return 1;
}

int rm_thread_started(int tid)
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    if (tid < 0 || tid >= N) {
        printf("invalid thread id supplied to rm_thread_started\n");
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
        printf("thread information cannot be found in rm_thread_ended\n");
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
        printf("thread information cannot be found in rm_claim\n");
        pthread_mutex_unlock(&lock);
        return -1;
    }
    
    if (DA == 1) {
        for (int i = 0; i < M; i++) {
            if (claim[i] > ExistingRes[i]) {
                printf("process claims more resources than existing\n");
                pthread_mutex_unlock(&lock);
                return -1;
            }
            maxDemand[tid][i] = claim[i];
            need[tid][i] = maxDemand[tid][i];
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
            maxDemand[i][j] = 0;
        }
    }

    if (pthread_mutex_init(&lock, NULL) != 0 ) {
        printf("mutex lock cannot be initialized\n");
        return -1;
    }
    if (pthread_cond_init(&cv, NULL) != 0) {
        printf("condition variable cannot be initialized\n");
        return -1;
    }

    return  (ret);
}

int canAllocate(int request[], int tid) {
    if (!isArraySmaller(request, ExistingRes, M))
        return 0;
    return 1;
}

int isAvailable(int request[]) {
    if (!isArraySmaller(request, available, M))
        return 0;
    return 1;
}

void allocate(int tid, int request[]) {
    for (int i = 0; i < M; i++) {
        available[i] -= request[i];
        allocation[tid][i] += request[i];
        if (DA == 1)
            need[tid][i] -= request[i];
    }
}

void deallocate(int tid, int release[]) {
    for (int i = 0; i < M; i++) {
        available[i] += release[i];
        allocation[tid][i] -= release[i];
        if (DA == 1)
            need[tid][i] = maxDemand[tid][i] - allocation[tid][i];
    }
}

int isSafe() {
    int work[M];
    int finish[N];
    for (int i = 0; i < M; i++)
        work[i] = available[i];
    for (int i = 0; i < N; i++)
        finish[i] = 0;
    
    int foundFlag = 1;
    while (foundFlag) {
        foundFlag = 0;
        for (int i = 0; i < N; i++) {
            if (!finish[i] && isArraySmaller(need[i], work, M)) {
                foundFlag = 1;
                for (int j = 0; j < M; j++) {
                    work[j] += allocation[i][j];
                }
                finish[i] = 1;
                break;
            }
        }
    }

    for (int i = 0; i < N; i++) {
        if (!finish[i])
            return 0;
    }
    return 1;
}

int isNextStateSafe(int tid, int request[]) {
    allocate(tid, request);
    if (!isSafe()) {
        deallocate(tid, request);
        return 0;
    }
    deallocate(tid, request);
    return 1;
}

int rm_request (int request[])
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    int tid = getSelectedID(pthread_self());
    if (tid == -1) {
        printf("thread information cannot be found in rm_request\n");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    if (!canAllocate(request, tid)) { // request exceeds max available resource instance
        printf("more resources requested than available\n");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    // maintain request matrix
    for (int i = 0; i < M; i++) {
        Request[tid][i] = request[i];
    }
        
    if (DA == 1) { // deadlock avoidance
        if (!isArraySmaller(request, need[tid], M)) { // request exceeded maximum claim
            printf("process has exceeded its maximum claim\n");
            pthread_mutex_unlock(&lock);
            return -1;
        }

        while (!isAvailable(request) || !isNextStateSafe(tid, request)) // resources are not available or next state is not safe
            pthread_cond_wait(&cv, &lock);
        
        allocate(tid, request);
    }
    else {
        while (!isAvailable(request)) // wait until resources become available
            pthread_cond_wait(&cv, &lock);
        allocate(tid, request);
    }

    for (int i = 0; i < M; i++) // the thread has been allocated all of its requests
        Request[tid][i] = 0;

    pthread_mutex_unlock(&lock);
    return(ret);
}


int rm_release (int release[])
{
    pthread_mutex_lock(&lock);
    int ret = 0;
    int tid = getSelectedID(pthread_self());

    // check if thread tries to deallocate more instances than allocated
    if (!isArraySmaller(release, allocation[tid], M)) {
        printf("thread tried to release more instances than allocated\n");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    deallocate(tid, release);
    // wake up sleeping threads
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
    return (ret);
}

int isRequestZero(int tid) {
    for (int i = 0; i < N; i++) {
        if (Request[tid][i] != 0)
            return 0;
    }
    return 1;
}

int rm_detection()
{
    pthread_mutex_lock(&lock);
    int work[M];
    int finish[N];

    for (int i = 0; i < M; i++)
        work[i] = available[i];

    for (int i = 0; i < N; i++) {
        if (!isRequestZero(i))
            finish[i] = 0;
        else
            finish[i] = 1;
    }

    int foundFlag = 1;
    while (foundFlag) {
        foundFlag = 0;
        for (int i = 0; i < N; i++) {
            if (!finish[i] && isArraySmaller(Request[i], work, M)) {
                foundFlag = 1;
                for (int j = 0; j < M; j++) {
                    work[j] += allocation[i][j];
                }
                finish[i] = 1;
                break;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < N; i++) {
        if (!finish[i])
            count++;
    }

    pthread_mutex_unlock(&lock);
    return count;
}


void rm_print_state (char hmsg[])
{
    pthread_mutex_lock(&lock);
    printf("#################################\n");
    printf("%s\n", hmsg);
    printf("#################################\n");

    printf("Exist:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n%-5s", "");

    for (int i = 0; i < M; i++) {
        printf("%-4d", ExistingRes[i]);
    }
    printf("\n\n");

    printf("Available:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n%-5s", "");

    for (int i = 0; i < M; i++) {
        printf("%-4d", available[i]);
    }
    printf("\n\n");




    printf("Allocation:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf("%s%d%-3s", "T", i, ":");
        for (int j = 0; j < M; j++) {
            printf("%-4d", allocation[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    printf("Request:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf("%s%d%-3s", "T", i, ":");
        for (int j = 0; j < M; j++) {
            printf("%-4d", Request[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    printf("MaxDemand:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf("%s%d%-3s", "T", i, ":");
        for (int j = 0; j < M; j++) {
            printf("%-4d", maxDemand[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    printf("Need:\n");
    printf("%-5s", "");
    for (int i = 0; i < M; i++)
        printf("%s%-3d", "R", i);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf("%s%d%-3s", "T", i, ":");
        for (int j = 0; j < M; j++) {
            printf("%-4d", need[i][j]);
        }
        printf("\n");
    }

    printf("#################################\n\n");
    pthread_mutex_unlock(&lock);
} 
