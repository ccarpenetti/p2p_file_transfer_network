#include "libb64-1.2/include/b64/cencode.h"
#include "libb64-1.2/include/b64/cdecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Note: this file is largely based on "c-example2.c" on https://github.com/BuLogics/libb64/blob/master/examples/c-example2.c

/* Note: I only created and call this function to perfectly match the 
example output on the Part 1 instructions. The libb64 library adds newlines to the encoded output,
but this made me produce a different Content-Length than that in the example output.
Out of caution, I removed this whitespace to perfectly match the example output */
void remove_whitespace(char *encoded, int *total) {
    char *src = encoded;  
    char *dst = encoded;  

    // Remove all whitespace from output
    while (*src) {
        if (!isspace(*src)) {
            *dst = *src;
            dst++;
        }
        else {
            *total = *total - 1;
        }
        src++;
    }

    *dst = '\0'; // force ending of string
}

void encode(FILE* inputFile, FILE *outputFile, int *enc_size)
{
	// Create buffer to hold encoded data
	int size = 100;
	char input[size];
	char encoded[2*size];

        memset(input, 0, size);
        memset(encoded, 0, 2*size);

	// Encoder state
	base64_encodestate s;

	// Count will store the number of bytes encoded by a single call
	int count = 0;
        int total = 0;
	
	// Initialize the encoder state
	base64_init_encodestate(&s);

	// Read data from the input file, encode it, and then write it to the output file
	while (1)
	{
		count = fread(input, sizeof(char), size, inputFile);

                // If there is no more data left to read, break from loop
		if (count == 0) {
                    break;
                }

		count = base64_encode_block(input, count, encoded, &s);
                fwrite(encoded, sizeof(char), count, outputFile);

                total += count;
                memset(input, 0, size);
                memset(encoded, 0, 2*size);
	}
        
        remove_whitespace(encoded, &total);
        *enc_size = total;	

        // We have reached the end of the input file, so finalize the encoding
        count = base64_encode_blockend(encoded, &s);
        fwrite(encoded, sizeof(char), count, outputFile);

}

void decode(FILE *inputFile, FILE* outputFile)
{

    //Initialize buffer
    int size = 100;
    char* encoded = (char*)malloc(size);
    char* decoded = (char*)malloc(1*size);
    
    // Decoder state
    base64_decodestate ds;

    //Number of bytes encoded by a single call
    int count = 0;
    
    // Initialise the decoder state
    base64_init_decodestate(&ds);

    //Read contents of input, decode it, and write to output 
    while (1)
    {
        count = fread(encoded, sizeof(char), size, inputFile);
        if (count == 0) {
            break;
        }
        count = base64_decode_block(encoded, count, decoded, &ds);

        // Write decoded bytes to outputFile
        fwrite(decoded, sizeof(char), count, outputFile);
    }   

 
    free(encoded);
    free(decoded); 
}

/*
int main(int argc, char *argv[]){

    //Test that the user only entered two arguments
    if ((argc <= 2) || (argc >= 4)){
        printf("Error: Incorrect number of arguments.\n");
        return 0;
    }

    // Open files to read and write
    FILE *inputFile = fopen(argv[2], "r");
    FILE *outputFile = fopen(strcat(argv[2], ".encoded"), "w");
    
    // Test if we opened files successfully    
    if ((inputFile == NULL) || (outputFile == NULL)) {
        printf("Error: Could not open one of more files.\n");
        return 0;
    }

    // Either encode or decode based on inputted operation
    if (strcmp(argv[1], "encode") == 0){
        encode(inputFile, outputFile);
    }
    else if (strcmp(argv[1], "decode") == 0) {
        printf("decode c