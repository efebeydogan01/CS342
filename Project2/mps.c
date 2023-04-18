#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include "list.c"

static void* schedule(void *param) {

}

int main(int argc, char* argv[]) {
    // get the start time of the program
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    int N = 2;
    char *SAP = "M";
    char *QS = "RM";
    char *ALG = "RR";
    int Q = 20;
    char *infile = "in.txt";
    int outmode = 1;
    char *outfile = "out.txt";

    int multiFlag = strcmp(SAP, "M") ? 1 : 0; // true if multiqueue approach is used

    // variables for single queue approach
    node_t *queue = NULL;
    pthread_mutex_t lock;
    // arrays to hold the queues and locks for the multiqueue approach
    node_t **queues;
    pthread_mutex_t *locks;
    if (multiFlag) {
        queues = (node_t **) malloc(sizeof(node_t *) * N);
        for (int i = 0; i < N; i++) {
            queues[i] = NULL;
        }
        locks = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t) * N);
    }

    pthread_t pid[N];
    for (int i = 0; i < N; i++) { // create N threads to simulate N processors
        int err = pthread_create(&pid[i], NULL, schedule, NULL);
        if (err) {
            printf("error occured: %d\n",  err);
            exit(1);
        }
    }
}