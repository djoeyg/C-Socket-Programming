#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()
#include <fcntl.h>

// Error function used for reporting issues
void error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1); 
}

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname) {
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));

    // The address should be network capable
    address->sin_family = AF_INET;

    // Store the port number
    address->sin_port = htons(portNumber);

    // Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname(hostname); 
    if (hostInfo == NULL) { 
        error("CLIENT: ERROR, no such host"); 
    }

    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

int main(int argc, char *argv[]) {

    /*
    * Decryption Client
    * args: 0 dec_client 1 ciphertext 2 key 3 port
    * connects to localhost
    */

    int socketFD, portNumber; 
    int msgCharsWritten, keyCharsWritten;
    int ackCharsRead, encCharsRead;
    struct sockaddr_in serverAddress;
    size_t bufLen = 256;
    char msgBuffer[bufLen];
    char keyBuffer[bufLen];
    char ackBuffer[bufLen];
    char encBuffer[bufLen];

    // Check usage & args
    if (argc < 4) { 
        fprintf(stderr,"USAGE: %s ciphertext key port\n", argv[0]); 
        exit(1); 
    }

    // Open the ciphertext file to get input message
    FILE *cipherTextFD = fopen(argv[1], "r");
    if (cipherTextFD == NULL) { 
        fprintf(stderr, "cannot open %s for input\n", argv[1]); 
        exit(1); 
    }

    // Open the key file to get encryption key
    FILE *keyFD = fopen(argv[2], "r");
    if (keyFD == NULL) { 
        fprintf(stderr, "cannot open %s for input\n", argv[2]); 
        exit(1); 
    }

    // Determine length of ciphertext and key files
    struct stat msgStat;
    struct stat keyStat;
    stat(argv[1], &msgStat);
    ssize_t msgLen = msgStat.st_size;
    stat(argv[2], &keyStat);
    ssize_t keyLen = keyStat.st_size;

    // Validate length of key compared to length of ciphertext
    if( keyLen < msgLen) {

            // key can be larger than message but not shorter
            fprintf(stderr, "Error: %s is too short\n", argv[2]); 
            exit(1);
        }

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); 
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){

        // Report connetion error with the attempted port, and set exit value to 2.
        fprintf(stderr, "Error: could not contact dec_server on port %s\n", argv[3]);
        exit(2);
    }

    // Establish handshake with server socket 
    msgCharsWritten = send(socketFD, "dec_client", 10, 0);
    if (msgCharsWritten < 0) {
        error("CLIENT: ERROR writing handshake ACK to socket");
    }

    // Clear out the acknowledgement msg buffer
    memset(ackBuffer, '\0', sizeof(ackBuffer));

    // Get acknowledgment message from server
    ackCharsRead = recv(socketFD, ackBuffer, sizeof(ackBuffer) - 1, 0); 
    if (ackCharsRead < 0){
        error("CLIENT: ERROR reading ACK from socket");
    }

    // Report connetion error with the attempted port, and set exit value to 2. 
    if (strcmp(ackBuffer, "dec_server") != 0 ) {
        fprintf(stderr, "Error: could not connect to %s on port %s\n", ackBuffer, argv[3]);
        close(socketFD);
        exit(2);
    }

    /********** Read, Send, Receive, Write data **********/

    // initialize buffers and count the bytes read into buffers
    ssize_t m = 0;
    ssize_t k = 0;

    while(m < msgLen) {

        memset(msgBuffer, '\0', sizeof(msgBuffer));
        memset(keyBuffer, '\0', sizeof(keyBuffer));
        m += fread(msgBuffer, sizeof(char), bufLen - 1, cipherTextFD);
        k += fread(keyBuffer, sizeof(char), bufLen - 1, keyFD);

        // Validate length of key compared to length of ciphertext
        if( k < m) {
            // key can be larger than message but not shorter
            fprintf(stderr, "Error: %s is too short\n", argv[2]); 
            exit(1);
        }

        // Remove the trailing \n at end of strings
        msgBuffer[strcspn(msgBuffer, "\n")] = '\0';
        keyBuffer[strcspn(keyBuffer, "\n")] = '\0';

        // Check for "bad" characters in ciphertext and key files
        for(int i = 0; i < strlen(msgBuffer); i++) {

            if(( msgBuffer[i] < 65 || msgBuffer[i] > 90) && msgBuffer[i] != 32) {
                // Send end of data message to server if bad character found and exit
                msgCharsWritten = send(socketFD, "STOP", 4, 0);
                if (msgCharsWritten < 0) {
                    error("CLIENT: ERROR writing 'end of data' to socket");
                }
                error("enc_client error: ciphertext contains bad characters\n");
            }
            if(( keyBuffer[i] < 65 || keyBuffer[i] > 90) && keyBuffer[i] != 32) {
                // Send end of data message to server if bad character found and exit
                msgCharsWritten = send(socketFD, "STOP", 4, 0);
                if (msgCharsWritten < 0) {
                    error("CLIENT: ERROR writing 'end of data' to socket");
                }
                error("enc_client error: key contains bad characters\n");
            }
        }

        // send message file contents from buffer to server socket
        do { 
            msgCharsWritten = send(socketFD, msgBuffer, strlen(msgBuffer), 0);

            if (msgCharsWritten < 0) {
                error("CLIENT: ERROR writing msg data to socket");
            }
            if (msgCharsWritten < strlen(msgBuffer)){
                fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
            }
        } while (msgCharsWritten < strlen(msgBuffer));

        // Clear out the acknowledgement msg buffer
        memset(ackBuffer, '\0', sizeof(ackBuffer));

        // Get acknowledgment message from server
        ackCharsRead = recv(socketFD, ackBuffer, sizeof(ackBuffer) - 1, 0); 
        if (ackCharsRead < 0) {
            error("CLIENT: ERROR reading ACK from socket");
        }

        // send key file contents from buffer to server socket
        do {
            keyCharsWritten = send(socketFD, keyBuffer, strlen(keyBuffer), 0);
            if (keyCharsWritten < 0){
                error("CLIENT: ERROR writing key data to socket");
            }
            if (keyCharsWritten < strlen(keyBuffer)){
                fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
            }
        } while (keyCharsWritten < strlen(keyBuffer));

        // Clear out the encrypted msg buffer
        memset(encBuffer, '\0', sizeof(encBuffer));

        // Get encrypted message back from server
        encCharsRead = recv(socketFD, encBuffer, sizeof(encBuffer) - 1, 0); 
        if (encCharsRead < 0){
            error("CLIENT: ERROR reading from socket");
        }
        fprintf(stdout, "%s", encBuffer);
        fflush(stdout);
    }

    // Send end of data message to server
    msgCharsWritten = send(socketFD, "STOP", 4, 0);
    if (msgCharsWritten < 0) {
        error("CLIENT: ERROR writing 'end of data' to socket");
    }

    // Close the socket, ciphertext and key files and return
    fclose(cipherTextFD);
    fclose(keyFD);
    close(socketFD); 
    return 0;
}