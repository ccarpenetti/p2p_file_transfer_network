#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAXDATASIZE 100

int client_fd; // socket of client

// Get socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {

    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Iterate through port argument to see if it contains all digits
bool check_port(char *port){
    
    while (*port) {

        if (isdigit(*port) == false) {
            return false;
        }
        port++;
    }

    return true;
}

// Check that the user enters only a singular word and not multiple words
bool check_string(char *str){

    int count = 0;
    int in_word = false;  // flag for whether we are inside a word

    // Count all words in the string
    while (*str) {

        if (isspace(*str)) {
            in_word = false;
        } 
        else if (in_word == false) {
            in_word = true;  // we are now entering a word
            count++;
        }
        str++;
    }

    if (count == 1) {
        return true;
    }
    else {
        return false;
    }
}

// Setup the client and connect to the server
int setup_client(char *host, char *port) {

    struct addrinfo hints, *servinfo, *p;

    int rv;

    char s[INET6_ADDRSTRLEN]; // store string form of address
    memset(s, 0, sizeof(s));
  
    // Initialize specifications for communication
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through results and connect to the first possible one
    for (p = servinfo; p != NULL; p = p->ai_next) {
    
        // Try to create socket
        if ((client_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }

        // Upon successful socket creation, establish 3-way handshake
        if (connect(client_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(client_fd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 1;
    }

    //Put IP address in string format
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));

    printf("Connected successfully for server at %s:%s\n", s, port);

    freeaddrinfo(servinfo);

    return 0;

}

int main(int argc, char *argv[]){

    // Check for correct number of arguments
    if (argc != 3) {
        printf("Error: incorrect number of arguments.\n");
        return 0;
    }

    // Check that the user entered a valid port value (i.e. a number)
    if (check_port(argv[2]) == false) {
        printf("Error: invalid port number.\n");
        return 1;
    }

    int numbytes;

    char buf[MAXDATASIZE];
    memset(buf, 0, sizeof(buf));

    // Setup client
    if (setup_client(argv[1], argv[2]) == 1){
        return 1;
    }

    //Wait for stdin
    while (1) {

        // Gather stdin
        fgets(buf, MAXDATASIZE, stdin);
        //printf("%s", buf);

        // Remove the newline from stdin
        buf[strlen(buf) - 1] = '\0';

        //printf("%s", buf);

        // Add a CR/LF
        strcat(buf, "\r\n");        
     
        //Send message to server      
        if (send(client_fd, buf, strlen(buf), 0) == -1) {
            perror("client send");
        }

        //Receive response from server
        printf("Received a response\n");
        while ((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) > 0) {

            //Force the termination of the string
            buf[numbytes] = '\0'; 

            // Print response from server
            printf("%s", buf);
            
        }

        if (numbytes == -1) {
            perror("client recv");
            return 1;
        }
    }

    // Close client socket
    close(client_fd);
    
    printf("Client exiting successfully!\n");

    return 0;
}   
