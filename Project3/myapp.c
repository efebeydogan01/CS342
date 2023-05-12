#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include "rm.h"

#define NUMR 5        // number of resource types
#define NUMP 5        // number of threads

int AVOID = 1;
int exist[NUMR] =  {1, 1, 1, 1, 1};  // resources existing in the system
pthread_mutex_t tlock;

void pr (int tid, char astr[], int m, int r[])
{
    pthread_mutex_lock(&tlock);
    int i;
    printf ("thread %d, %s, [", tid, astr);
    for (i=0; i<m; ++i) {
        if (i==(m-1))
            printf ("%d", r[i]);
        else
            printf ("%d,", r[i]);
    }
    printf ("]\n");
    pthread_mutex_unlock(&tlock);
}


void setarray (int r[MAXR], int m, ...)
{
    va_list valist;
    int i;
    
    va_start (valist, m);
    for (i = 0; i < m; i++) {
        r[i] = va_arg(valist, int);
    }
    va_end(valist);
    return;
}

void *threadfunc(void *a)
{
    int tid;
    int request1[MAXR];
    int request2[MAXR];
    int claim[MAXR];
    
    tid = *((int*)a);
    rm_thread_started (tid);

    setarray(claim, NUMR, 0, 0, 0, 0, 0);
    claim[tid] = 1;
    claim[(tid + 1) % NUMR] = 1;
    rm_claim(claim);
    
    setarray(request1, NUMR, 0, 0, 0, 0, 0);
    request1[tid] = 1;
    pr (tid, "REQ", NUMR, request1);
    rm_request (request1);

    sleep(4);

    setarray(request2, NUMR, 0, 0, 0, 0, 0);
    request2[(tid + 1) % NUMR] = 1;
    pr (tid, "REQ", NUMR, request2);
    rm_request (request2);

    rm_release (request1);
    rm_release (request2);

    rm_thread_ended();
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int i;
    int tids[NUMP];
    pthread_t threadArray[NUMP];
    int count;
    int ret;
    pthread_mutex_init(&tlock, NULL);

    if (argc != 2) {
        printf ("usage: ./app avoidflag\n");
        exit (1);
    }

    AVOID = atoi (argv[1]);
    
    printf("This example program simulates the Dining Philosophers Problem to demonstrate the occurence, detection, and avoidance of the deadlocks.\n \
    There are 5 threads and 5 resources. Each thread first requests and gets the resource at their index, and then the one on their right. Since the second round of requests cannot be successful, each thread becomes deadlocked.\n");

    if (AVOID == 1)
        rm_init (NUMP, NUMR, exist, 1);
    else
        rm_init (NUMP, NUMR, exist, 0);

    for (int i = 0; i < NUMP; i++) {
        tids[i] = i;
        pthread_create (&(threadArray[i]), NULL,
                        (void *) threadfunc, (void *)
                        (void*)&tids[i]);
    }
    
    count = 0;
    while ( count < 10) {
        sleep(1);
        if (count == 0)
            rm_print_state("The initial state");
        else {
            ret = rm_detection();
            if (ret > 0) {
                printf ("deadlock detected, count=%d\n", ret);
                rm_print_state("State after deadlock");
                break;
            }
        }
        count++;
    }
    printf("ret:%d\n", ret);

    if (ret == 0) {
        for (i = 0; i < NUMP; ++i) {
            pthread_join (threadArray[i], NULL);
            printf ("joined\n");
        }
    }
    else {
        printf("stopping execution with %d deadlocked threads", ret);
    }
}


