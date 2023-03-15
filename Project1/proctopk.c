#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include<time.h>
#include "helpers.c"
#define SNAME "shmname"

// run instructions:
// g++ proctopk.c -o proctopk (or type make)
// ./proctopk K outfile N ...fileNames...
// sample valgrind code: valgrind --leak-check=full --show-leak-kinds=all ./proctopk 10 out.txt 2 in1.txt in2.txt

int main( int argc, char* argv[]) {
    clock_t start, end;
    double execution_time;
    start = clock();

    int K = atoi(argv[1]); // number of words to find
    char* outfile = argv[2]; // name of the output file that will store the result
    int N = atoi(argv[3]); // number of input files
    const int NUM_CHARS = 64;
    // shared memory will store the word/freq pairs
    const int SIZE = K*N*sizeof(struct WordFreqPairs);
    const int INITIAL_ARRAY_SIZE = 2;
    char fileNames[N][NUM_CHARS]; // array to hold the names of input files
    for (int i = 0; i < N; i++) {
        strcpy( fileNames[i], argv[i+4]);
    }

    // Creating shared memory for processes
    int shm_fd; 
    WordFreqPairs *shmem;
    shm_fd = shm_open(SNAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SIZE); // set size of shared memory
    shmem = (WordFreqPairs *) mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem == MAP_FAILED) { printf("Map failed\n"); return -1; }

    for (int i = 0; i < N; i++) {
        int child = fork();
        if (child == 0) { // in child process
            WordStruct wordStruct;
            createWordStruct(&wordStruct, INITIAL_ARRAY_SIZE);
            char* fileName = fileNames[i];
            readFile(fileName, &wordStruct);
            int size = wordStruct.curSize;
            WordFreqPairs* wordFreqPairs = (WordFreqPairs*) malloc(size * sizeof(WordFreqPairs));
            // returns the array of word-freq pairs in descending sorted order
            sortFreqs(&wordStruct, wordFreqPairs);
            int minimum = K < size ? K : size;

            if (minimum == 0) { // if the file is empty, that block of memory is not valid
                shmem[i*K].valid = 0;
            }

            // copy the word and frequency pairs into the shared memory
            for (int j = 0; j < minimum; j++) {
                // store the number of words in the first word/freq pair
                if (j == 0) {
                    wordFreqPairs[j].noWords = minimum;
                    wordFreqPairs[j].valid = 1;
                }
                // 0 1 2 ... K - 1 K K+1 ... 2K-1
                shmem[j + i*K] = wordFreqPairs[j];
                strcpy(shmem[j + i*K].word, wordFreqPairs[j].word);
            }

            freeMemory(&wordStruct);
            free(wordFreqPairs);
            exit(0);
        }
    }
    // back to parent process
    for (int i = 0; i < N; i++) {
        wait(NULL); // wait for each child process to end 
    }
    
    WordStruct res;
    createWordStruct(&res, INITIAL_ARRAY_SIZE);
    for (int i = 0; i < N; i++) {
        if (shmem[i*K].valid) { // do this if the file wasn't empty
            int noWords = shmem[i*K].noWords;

            for (int j = 0; j < noWords; j++) {
                addWord(&res, shmem[j + i*K].word, shmem[j + i*K].freq);
            } 
        }
    }

    // sort the words based on frequency
    int size = res.curSize;
    WordFreqPairs* wordFreqPairs = (WordFreqPairs*) malloc(size * sizeof(WordFreqPairs));
    sortFreqs(&res, wordFreqPairs);
    // check if there are K words or not
    int minimum = K < size ? K : size;

    FILE *fptr = fopen(outfile, "w");
    if (!fptr) {
        printf("cannot open output file");
        exit(1);
    }

    for (int i = 0; i < minimum; i++) {
        if (i == minimum - 1) {
            fprintf(fptr, "%s %d", wordFreqPairs[i].word, wordFreqPairs[i].freq);
        }
        else {
            fprintf(fptr, "%s %d\n", wordFreqPairs[i].word, wordFreqPairs[i].freq);
        }
    }

    fclose(fptr);
    free(wordFreqPairs);
    freeMemory(&res);
    // remove the shared memory
    if (shm_unlink(SNAME) == -1) {
        printf("Error removing %s\n",SNAME); 
        exit(-1);
    }

    end = clock();
    execution_time = ((double)(end - start))/CLOCKS_PER_SEC;
    printf("Time taken to execute in seconds : %f\n", execution_time);
    return 0;
}