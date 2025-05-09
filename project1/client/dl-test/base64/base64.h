#ifndef BASE64
#define BASE64

#include <stdio.h>

// Function declaration
void encode(FILE* inputFile, FILE* outputFile, int *enc_size);
void decode(FILE *inputFile, FILE* outputFile);

#endif

