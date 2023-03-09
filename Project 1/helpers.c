#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define WORD_SIZE 64

typedef struct WordStruct {
    char** words;
    int* frequencies;
    int curSize; // keep track of the number of items in the array
    int maxSize; // keep track of max size so know when to expand array
} WordStruct;

typedef struct WordFreqPairs {
    char* word;
    int freq;
} WordFreqPairs;

void createWordStruct(WordStruct* wordStruct, int arraySize) {
    wordStruct->words = malloc( arraySize * sizeof(char));
    wordStruct->frequencies = malloc( arraySize * sizeof(int));
    wordStruct->curSize = 0;
    wordStruct->maxSize = arraySize;
}

void addWord(WordStruct* wordStruct, char* word) {
    // check if the word already exists in the frequencies array
    int exists = 0;
    int i;
    for (i = 0; i < wordStruct->curSize; i++) {
        if (strcmp(wordStruct->words[i], word) == 0) 
            exists = 1;
            break;
    }
    // add the word to the list if it isn't already added
    if (!exists) {
        // if there isn't enough space in the array, expand it
        if (wordStruct->maxSize <= wordStruct->curSize) {
            wordStruct->maxSize = wordStruct->maxSize * 2;
            wordStruct->words = realloc(wordStruct->words, wordStruct->maxSize * sizeof(char*));
            wordStruct->frequencies = realloc(wordStruct->frequencies, wordStruct->maxSize * sizeof(int));
        }
        // allocate space for new word
        wordStruct->words[wordStruct->curSize] = malloc(sizeof(char) * (strlen(word) + 1));
        strcpy(wordStruct->words[wordStruct->curSize], word);
        wordStruct->curSize = wordStruct->curSize + 1;
    }
    else {
        // if the word already exists, increase its frequency count
        wordStruct->frequencies[i - 1]++;
    }
}

void readFile(char* fileName, WordStruct* wordStruct) {
    FILE* fptr;
    char word[WORD_SIZE];
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
        if ( ch != ' ' && ch != '\t' && ch != '\n' && ch != EOF) {
            ch = toupper(ch);
            strncat(curWord, &ch, 1);
        }
        else {
            if (strlen(curWord) > 0) {
                addWord( wordStruct, curWord);
            }
            strcopy(curWord, "");
        }
    } while (ch != EOF);
    fclose(fptr);
}

void sortFreqs(WordStruct* wordStruct, WordFreqPairs* pairs) {
    // a function to sort word frequencies, and the word array accordingly
    int wordNum = wordStruct->curSize;
    // the structs will be sorted
    for (int i = 0; i < wordNum; i++) {
        pairs[i].word =  wordStruct->words[i];
        pairs[i].freq = wordStruct->frequencies[i];
    }

    // sort the pairs based on frequency
    qsort(pairs, wordNum, sizeof(struct WordFreqPairs), sortHelper);
}

int sortHelper(const void *a, const void *b) {
    // callback function for qsort in sortFreqs() function
    int x = *(int*)a;
    int y = *(int*)b;
    return (x < y) - (x > y);
}

void freeMemory(WordStruct* wordStruct) {
    for (int i = 0; i < wordStruct->curSize; i++) {
        free(wordStruct->words[i]);
    }
    free(wordStruct->words);
    free(wordStruct->frequencies);
}