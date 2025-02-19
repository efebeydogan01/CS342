#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define WORD_SIZE 64
#define MAX_K 1000

typedef struct WordStruct {
    char** words;
    int* frequencies;
    int curSize; // keep track of the number of items in the array
    int maxSize; // keep track of max size so know when to expand array
} WordStruct;

typedef struct WordFreqPairs {
    char word[WORD_SIZE];
    int freq;
} WordFreqPairs;

typedef struct WordFreqArray {
    int valid;
    int size;
    WordFreqPairs arr[MAX_K];
} WordFreqArray;

void createWordStruct(WordStruct* wordStruct, int arraySize) {
    wordStruct->words = (char**) malloc( arraySize * sizeof(char*));
    wordStruct->frequencies = (int*) malloc( arraySize * sizeof(int));
    wordStruct->curSize = 0;
    wordStruct->maxSize = arraySize;
}

void createWordFreqArray(WordFreqPairs *pairs, WordFreqArray *wordFreqArray, int min) {
    // copy the words into shared memory
    for (int i = 0; i < min; i++) {
        // set the frequency of the word
        wordFreqArray->arr[i].freq = pairs[i].freq;
        // copy the word
        strcpy(wordFreqArray->arr[i].word, pairs[i].word);
    }
    wordFreqArray->valid = 1;
    wordFreqArray->size = min;
}

void addWord(WordStruct* wordStruct, char* word, int frequency) {
    // check if the word already exists in the words array
    int isUnique = 1;
    int i;
    for (i = 0; i < wordStruct->curSize; i++) {
        if (strcmp(wordStruct->words[i], word) == 0) {
            isUnique = 0;
            break;
        }
    }
    // add the word to the list if it isn't already added
    if (isUnique) {
        // if there isn't enough space in the array, expand it
        if (wordStruct->maxSize <= wordStruct->curSize) {
            wordStruct->maxSize = wordStruct->maxSize * 2;
            wordStruct->words = (char**) realloc(wordStruct->words, wordStruct->maxSize * sizeof(char*));
            wordStruct->frequencies = (int*) realloc(wordStruct->frequencies, wordStruct->maxSize * sizeof(int));
        }
        // allocate space for new word
        wordStruct->words[wordStruct->curSize] = (char*) malloc(sizeof(char) * (strlen(word) + 1));
        strcpy(wordStruct->words[wordStruct->curSize], word);
        wordStruct->frequencies[wordStruct->curSize] = frequency;
        wordStruct->curSize = wordStruct->curSize + 1;
    }
    else {
        // if the word already exists, increase its frequency count
        wordStruct->frequencies[i] += frequency;
    }
}

void readFile(char* fileName, WordStruct* wordStruct) {
    FILE* fptr;
    // open file
    fptr = fopen(fileName, "r");
    // exit if file cannot be opened
    if (!fptr) {
        printf("Unable to open file.\n");
        exit(EXIT_FAILURE);
    }

    char curWord[WORD_SIZE] = "";
    char ch;
    do {
        ch = fgetc(fptr);
        if ( !isspace(ch) && ch != EOF) {
            ch = toupper(ch);
            strncat(curWord, &ch, 1);
        }
        else {
            if (strlen(curWord) > 0) {
                addWord( wordStruct, curWord, 1);
            }
            strcpy(curWord, "");
        }
    } while (ch != EOF);
    fclose(fptr);
}

int sortHelper(const void *a, const void *b) {
    // callback function for qsort in sortFreqs() function

    if ( ((const struct WordFreqPairs *) a)->freq < ((const struct WordFreqPairs *) b)->freq) {
        return 1;
    }
    else if ( ((const struct WordFreqPairs *) a)->freq > ((const struct WordFreqPairs *) b)->freq) {
        return 0;
    }
    // the frequencies are equal
    int result = strcmp(((const struct WordFreqPairs *) a)->word, ((const struct WordFreqPairs *) b)->word);
    if (result <= 0) // first word is lexicographically smaller so it should be chosen, the words cannot be the same
        return 0;
    return 1;
}

void sortFreqs(WordStruct* wordStruct, WordFreqPairs* pairs) {
    // a function to sort word frequencies, and the word array accordingly
    int wordNum = wordStruct->curSize;
    // the structs will be sorted
    for (int i = 0; i < wordNum; i++) {
        strcpy(pairs[i].word, wordStruct->words[i]);
        pairs[i].freq = wordStruct->frequencies[i];
    }

    // sort the pairs based on frequency
    qsort(pairs, wordNum, sizeof(struct WordFreqPairs), sortHelper);
}

void freeMemory(WordStruct* wordStruct) {
    for (int i = 0; i < wordStruct->curSize; i++) {
        free(wordStruct->words[i]);
    }
    free(wordStruct->words);
    free(wordStruct->frequencies);
}