#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

// Global variable tracks number of child processes
static volatile sig_atomic_t CHILD_SOCKETS = 0;

// Error function to report issues and exit process
void error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1); 
}

/* use signal handler to reap all dead processes
*  Source citation: Beej's Guide To Network Programming, Section 6.1 A Simple Stream Server
*  https://beej.us/guide/bgnet/html/#a-simple-stream-server
*/
void sigchld_handler(int signo)
{
    // Decrement number of active child processes for each process reaped
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0) {
        CHILD_SOCKETS--;
    }
    errno = saved_errno;
}

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, int portNumber) {
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 
    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

int main(int argc, char *argv[]) {
    int connectionSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    // Check usage & args
    if (argc < 2) { 
        fprintf(stderr,"USAGE: %s port\n", argv[0]); 
        exit(1);
    }

    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        error("ERROR on binding");
    }

    // Start listening for connetions. Allow up to 5 connections to queue up
    listen(listenSocket, 5);

    /* use signal handler to reap all dead processes
     * Source citation: Beej's Guide To Network Programming, Section 6.1 A Simple Stream Server
     * https://beej.us/guide/bgnet/html/#a-simple-stream-server
    */
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        error("sigaction");
    }

    int msgCharsRead, keyCharsRead, ackCharsRead;
    int ackCharsWritten, encCharsWritten;
    size_t bufLen = 256;
    char msgBuffer[bufLen];
    char keyBuffer[bufLen];
    char ackBuffer[bufLen];
    char encBuffer[bufLen];
  
    // Accept a connection, blocking if one is not available until one connects
    while(1) {
    
        // Accept only up to 5 socket connections at one time
        if (CHILD_SOCKETS <= 5) {

            // Accept the connection request which creates a connection socket
            connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); 
            if (connectionSocket < 0){
                error("ERROR on accept");
            }

            /* Handle each client request in a new child process
            *  Source Citation: The Linux Programming Interface, Author: Michael Kerrisk
            *  Chapter 60.3 - Listing 60.4, page 1,245
            */
            switch (fork()) {
            case -1:
                fprintf(stderr, "Can't create child process\n");
                close(connectionSocket);
                break;

            // Child Process
            case 0:
                // increment count of active child processes                       
                CHILD_SOCKETS++;

                // Close unneeded copy of listening socket
                close(listenSocket);        

                memset(ackBuffer, '\0', sizeof(ackBuffer));

                // Establish handshake with client socket
                ackCharsRead = recv(connectionSocket, ackBuffer, sizeof(ackBuffer) - 1, 0); 
                if (ackCharsRead < 0){
                    error("ERROR reading from socket");
                }

                // Send servier identification acknowledgment back to the client
                ackCharsWritten = send(connectionSocket, "enc_server", 10, 0); 
                if (ackCharsWritten < 0){
                    error("ERROR writing to socket");
                }

                // Refuse and close connection to wrong client
                if (strcmp(ackBuffer, "enc_client") != 0 ) {
                    close(connectionSocket);
                    exit(2);
                }

                while(1) {

                    memset(msgBuffer, '\0', sizeof(msgBuffer));

                    // Read the client's message from the socket
                    msgCharsRead = recv(connectionSocket, msgBuffer, sizeof(msgBuffer) - 1, 0); 
                    if (msgCharsRead < 0){
                        error("ERROR reading from socket");
                    }

                    // Stop processing data message sent from client
                    if (strcmp(msgBuffer, "STOP") == 0 ) {
                        break;
                    } 

                    // Send an acknowledgment message back to the client
                    ackCharsWritten = send(connectionSocket, "Server acknowledgment", 21, 0); 
                    if (ackCharsWritten < 0){
                        error("ERROR writing to socket");
                    }

                    memset(keyBuffer, '\0', sizeof(keyBuffer));

                    // Read the client's encryption key from the socket
                    keyCharsRead = recv(connectionSocket, keyBuffer, sizeof(keyBuffer) - 1, 0); 
                    if (keyCharsRead < 0){
                        error("ERROR reading from socket");
                    }

                    if( keyCharsRead < msgCharsRead) {
                        // key can be larger than message but not shorter
                        error("Error: key is too short"); 
                    }

                    /********** Encrypt message & send back data to client **********/ 

                    char charList[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
                    memset(encBuffer, '\0', sizeof(encBuffer));
                    int errCh, msgCh, keyCh, encCh;

                    for(int i = 0; i < strlen(msgBuffer); i++) {

                        errCh = msgBuffer[i];
                        // Check for "bad" characters in plaintext file
                        if(( errCh < 65 || errCh > 90) && errCh != 32) {
                            error("enc_server error: plaintext contains bad characters");
                        }

                        if( msgBuffer[i] == ' ') {
                            msgCh = 26;
                        } else {
                            msgCh = (msgBuffer[i] - 65);
                        }

                        errCh = keyBuffer[i];
                        // Check for "bad" characters in key file
                        if(( errCh < 65 || errCh > 90) && errCh != 32) {
                            error("enc_server error: key contains bad characters");
                        }

                        if( keyBuffer[i] == ' ') {
                            keyCh = 26;
                        } else {
                            keyCh = (keyBuffer[i] - 65);
                        }

                        // put new encryped character into buffer
                        encCh = msgCh + keyCh;
                        encBuffer[i] = charList[encCh % 27];
                    }

                    // Add newline character to end of encoded string
                    if( strlen(msgBuffer) < bufLen - 1) {
                        encBuffer[strlen(msgBuffer)] = '\n';
                    }

                    // Send encrypted message back to the client
                    do {
                        encCharsWritten = send(connectionSocket, encBuffer, strlen(encBuffer), 0); 
                        if (encCharsWritten < 0){
                            error("ERROR Server writing to socket");
                        }
                        if (encCharsWritten < strlen(encBuffer)){
                            fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
                        }
                    } while (encCharsWritten < strlen(encBuffer));
                }
                // Close the connection socket for this client
                close(connectionSocket); 
                _exit(EXIT_SUCCESS);

            // Parent Process
            default:
                // Close unneeded copy of connected socket
                close(connectionSocket);

                // Loop to accept next connection
                break;                      
            }
        }
    }
    // Close the listening socket
    close(listenSocket); 
    return 0;
}
