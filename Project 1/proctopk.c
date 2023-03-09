#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include "helpers.c"
#define SNAME "shmname"

// run instructions:
// g++ proctopk.c -o main
// ./main K outfile N ...fileNames...

int main( int argc, char* argv[]) {
    int K = atoi(argv[1]); // number of words to find
    char* outfile = argv[2]; // name of the output file that will store the result
    int N = atoi(argv[3]); // number of input files
    // SHOULD THIS BE 64 OR 63?
    const int NUM_CHARS = 64;
    // shared memory will store the number of words returned by each process + the word/freq pairs
    const int SIZE = N*sizeof(int) + K*N*sizeof(struct WordFreqPairs);
    const int INITIAL_ARRAY_SIZE = 2;
    char fileNames[N][NUM_CHARS]; // array to hold the names of input files

    for (int i = 0; i < N; i++) {
        strcpy( fileNames[i], argv[i+4]);
    }

    // Creating shared memory for processes
    int shm_fd; 
    void *shmem;
    shm_fd = shm_open(SNAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd,SIZE); // set size of shared memory
    shmem = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem == MAP_FAILED) { printf("Map failed\n"); return -1; }

    for (int i = 0; i < N; i++) {
        int child = fork();
        if (child == 0) { // in child process
            WordStruct wordStruct;
            createWordStruct(&wordStruct, INITIAL_ARRAY_SIZE);
            char* fileName = fileNames[i];
            readFile(fileName, &wordStruct);

            WordFreqPairs wordFreqPairs[wordStruct.curSize];
            // returns the array of word-struct pairs in descending sorted order
            sortFreqs(&wordStruct, &wordFreqPairs);

            int minimum = min(K, wordStruct.curSize);
            // copy the word and frequency pairs into the shared memory
            for (int j = 0; j < minimum; j++) {
                memcpy(shmem + i*)
            }

        }
    }


    return 0;
}