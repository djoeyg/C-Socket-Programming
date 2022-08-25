#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* 
 * This program creates a key file of specified length. The characters in the 
 * file produced will be generated using the rand() function. The 27 allowed
 * characters are the 26 capital letters, and the space character. A new line
 * character is also added to the end of the key string.
 */

int main(int argc, char *argv[]){

    // Check usage & args
    if (argc != 2) { 
        fprintf(stderr,"USAGE: %s keylength\n", argv[0]); 
        exit(1);
    }

    // Initialize function variables and arrays
    int keyLen = atoi(argv[1]);
    char charList[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    char* newKey = malloc(sizeof(char)*(keyLen + 2));
    memset(newKey, '\0', sizeof(char)*(keyLen + 2));

    /* 
     * srand() sets the seed used by rand() to generate “random” numbers.
     * Standard practice is to call srand(time(0)) to set the seed value.
     * https://www.geeksforgeeks.org/rand-and-srand-in-ccpp/
     */

    srand(time(0));

    for(int i = 0; i < keyLen; i++) {
        newKey[i] = charList[rand() % 27];
    }
    newKey[keyLen] = '\n';
    fprintf(stdout, "%s", newKey);
    //printf("%lu\n", strlen(newKey));
    fflush(stdout);
    free(newKey);
    return 0;
}
