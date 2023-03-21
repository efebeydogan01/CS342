#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/time.h>
#include "helpers.c"

// run instructions:
// g++ threadtopk.c -o threadtopk (or type make)
// ./threadtopk K outfile N ...fileNames...

// global variables for threads
char** fileNames; // array to hold the names of input files
WordStruct *wordStruct; // holds the words and frequencies read by every thread
WordFreqArray *wordFreqArray; // holds the K most frequent words read by every thread
int K; // command line argument
const int INITIAL_ARRAY_SIZE = 2;

// callback method to be called by every thread
static void* processFiles(void *param) {
    int arrayIndex = *((int *) param);

    // create WordStruct
    createWordStruct(&wordStruct[arrayIndex], INITIAL_ARRAY_SIZE);
    readFile(fileNames[arrayIndex], &wordStruct[arrayIndex]); // read the file corresponding to the index of this array

    int size = wordStruct[arrayIndex].curSize;
    // create temp pair struct to store the sorted word/freq pairs
    WordFreqPairs *tempPair = (WordFreqPairs *) malloc(size * sizeof(WordFreqPairs)); 
    // returns the array of word-freq pairs in descending sorted order
    sortFreqs(&wordStruct[arrayIndex], tempPair);
    int minimum = K < size ? K : size; // find if there are K most frequent words or not

    if (minimum <= 0) { // if the file is empty, that block of memory is not valid
        wordFreqArray[arrayIndex].valid = 0;
    }
    else {
        createWordFreqArray(tempPair, &wordFreqArray[arrayIndex], minimum);
    }

    free(tempPair);
    pthread_exit(NULL);
}

int main( int argc, char* argv[]) {
    struct timeval start;
    struct timeval end;
    gettimeofday(&start, 0);

    K = atoi(argv[1]); // number of words to find
    char* outfile = argv[2]; // name of the output file that will store the result
    int N = atoi(argv[3]); // number of input files
    const int NUM_CHARS = 64;

    // allocate space for fileNames array
    fileNames = (char**) calloc(N, NUM_CHARS * sizeof(char));
    for (int i = 0; i < N; i++) {
        fileNames[i] = argv[i+4];
    }

    // allocate space for WordStruct array
    wordStruct = (WordStruct *) calloc(N, sizeof(WordStruct));
    // allocate space for wordFreqArray
    wordFreqArray = (WordFreqArray *) calloc(N, sizeof(struct WordFreqArray));

    pthread_t pid[N]; // array to hold thread ids
    int arrayIndex[N]; // array to hold the array indices of every thread
    for (int i = 0; i < N; i++) { // create N threads to process N files
        arrayIndex[i] = i;
        int err = pthread_create(&pid[i], NULL, processFiles, (void *) &arrayIndex[i]);
        if (err) {
            printf("error occured: %d\n",  err);
            exit(1);
        }
    }
    
    // back to main thread
    // join the threads
    for (int i = 0; i < N; i++) {
        if ( pthread_join( pid[i], NULL)) {
            printf("Error joining threads!");
            exit(1);
        }
    }
    WordStruct res;
    createWordStruct(&res, INITIAL_ARRAY_SIZE);
    for (int i = 0; i < N; i++) {
        if (wordFreqArray[i].valid) { // do this if the file wasn't empty
            int noWords = wordFreqArray[i].size;

            for (int j = 0; j < noWords; j++) {
                addWord(&res, wordFreqArray[i].arr[j].word, wordFreqArray[i].arr[j].freq);
            }
        }
    }

    // sort the words based on frequency
    int size = res.curSize;
    WordFreqPairs *temp = (WordFreqPairs *) malloc(size * sizeof(WordFreqPairs));
    sortFreqs(&res, temp);
    // check if there are K words or not
    int minimum = K < size ? K : size;

    FILE *fptr = fopen(outfile, "w");
    if (!fptr) {
        printf("cannot open output file");
        exit(1);
    }

    for (int i = 0; i < minimum; i++) {
        if (i == minimum - 1) {
            fprintf(fptr, "%s %d", temp[i].word, temp[i].freq);
        }
        else {
            fprintf(fptr, "%s %d\n", temp[i].word, temp[i].freq);
        }
    }
    fclose(fptr);
    free(temp);

    // free wordStruct
    for (int i = 0; i < N; i++) {
        freeMemory(&wordStruct[i]);
    }
    free(wordFreqArray);
    free(wordStruct);
    free(fileNames);
    freeMemory(&res);

    gettimeofday(&end, 0);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    printf("Time taken to execute in ms : %.2f\n", ((float) micros) / 1000);
    return 0;
}