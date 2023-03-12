#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "helpers.c"

// run instructions:
// g++ threadtopk.c -o threadtopk
// ./threadtopk K outfile N ...fileNames...

// global variables for threads
char** fileNames; // array to hold the names of input files
WordStruct *wordStruct; // holds the words and frequencies read by every thread
WordFreqPairs **wordFreqPairs; // holds the K most frequent words read by every thread
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

    wordFreqPairs[arrayIndex] = (WordFreqPairs*) malloc(minimum * sizeof(WordFreqPairs));

    // copy the word and frequency pairs into the global pairs array
    for (int i = 0; i < minimum; i++) {
        // store the number of words in the first word/freq pair
        if (i == 0) {
            tempPair[i].noWords = minimum;
        }

        wordFreqPairs[arrayIndex][i] = tempPair[i];
        strcpy(wordFreqPairs[arrayIndex][i].word, tempPair[i].word);
    }
    free(tempPair);
}

int main( int argc, char* argv[]) {
    K = atoi(argv[1]); // number of words to find
    char* outfile = argv[2]; // name of the output file that will store the result
    int N = atoi(argv[3]); // number of input files
    // SHOULD THIS BE 64 OR 63?
    const int NUM_CHARS = 64;

    // allocate space for fileNames array
    fileNames = (char**) calloc(N, NUM_CHARS * sizeof(char));
    for (int i = 0; i < N; i++) {
        fileNames[i] = argv[i+4];
    }

    // allocate space for WordStruct array
    wordStruct = (WordStruct *) calloc(N, sizeof(WordStruct));
    // allocate space for WordFreqPairs array
    wordFreqPairs = (WordFreqPairs **) calloc(N*K, sizeof(WordFreqPairs*));

    pthread_t pid[N]; // array to hold thread ids
    int arrayIndex[N]; // array to hold the array indices of every thread
    for (int i = 0; i < N; i++) { // create N threads to process N files
        arrayIndex[i] = i;
        int err;
        if (err = pthread_create(&pid[i], NULL, processFiles, (void *) &arrayIndex[i])) {
            printf("error occured: %d\n",  err);
            exit(1);
        }
    }

    
    // free wordStruct
    // free wordFreqPairs
    free(fileNames);
    return 0;
}