#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "../index/index.h"
#include <fnmatch.h>

#define BACKLOG 10
#define MAXDATASIZE 256

// Globals
int server_fd; //Server socket
FILE *index_file_read;

// Handler for CTRL-C
void sig_handler(int s) {

    printf("Control-C detected - shutting down server\n");
    close(server_fd);
    exit(0);

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

// Get socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {

    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Set up server, include creating socket, binding, and starting to listen
int setup_server(char *port) {

    int rv;
    int yes = 1;

    struct addrinfo hints, *servinfo, *p; 
 
    // Initialize specifications for communication
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Connect signal handler
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // Get address info that matches port and hints
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {

        // If a socket cannot be created with the current address information struct, continue
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
    
        // If you cannot set the socket options, raise error
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return 1;
        }

        // If you fail to bind, raise error and try another address
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_fd);
            perror("server: bind");
            continue;
        }

        break;

    }

    freeaddrinfo(servinfo);

    if (p == NULL){
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }


    return 0;

}

// Given the pattern to match, find all files that match that pattern
int find_files(char *pattern, int num_lines, char *ptr_arr[]) {

    int i = 0;

    // Create the full pattern
    char full_pattern[MAXDATASIZE];
    sprintf(full_pattern, "*/%s;*", pattern);

    printf("Pattern: %s\n", full_pattern);

    // If we read the file previously, set the file pointer back to the start of file
    fseek(index_file_read, 0, SEEK_SET);

    char line[MAXDATASIZE];

    // Iterate through all of file to see which files match the pattern
    while (fgets(line, sizeof(line), index_file_read)){
        if (fnmatch(full_pattern, line, 0) == 0) {
            ptr_arr[i] = strdup(line);
            if (ptr_arr[i] == NULL) {
                perror("strdup failed");
                return -1;
            }
            i++;
        }   
    }

    return i;

}   

int main(int argc, char *argv[]){

    // Check for correct number of arguments
    if (argc != 3) {
        printf("Error: incorrect number of arguments.\n");
        return 1;
    }

    //Check that the user entered a valid port value (i.e. a number)
    if (check_port(argv[1]) == false) {
        printf("Error: invalid port number.\n");
        return 1;
    }

    // Index the specified directory
    int num_lines = 0;
    FILE *index_file_write = fopen("index.txt", "w");
    
    num_lines = get_index(argv[2], strlen(argv[2]), index_file_write);

    fclose(index_file_write);

    if (num_lines == -1) {
        printf("Error: could not create index.\n");
        return 1;
    }

    // Open file to read from
    index_file_read = fopen("index.txt", "r");

    int numbytes;

    int client_fd; //New connection with client_fd
    struct sockaddr_storage their_addr; // hold's the client's address information
    
    char buf[MAXDATASIZE]; //will hold incoming messages
    memset(buf, 0, sizeof(buf));

    char response_headers[MAXDATASIZE]; // will hold our response_headers messages
    memset(response_headers, 0, sizeof(response_headers));

    char s[INET6_ADDRSTRLEN]; // store string form of address
    memset(s, 0, sizeof(s));

    // Setup server
    if (setup_server(argv[1]) == 1) {
        return 1;
    }

    printf("Server successfully started listening on Port %s\n", argv[1]);

    socklen_t sin_size;    

    // Main accept loop
    while(1) {

        printf("Waiting for new connections...\n");

        sin_size = sizeof(their_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&their_addr, &sin_size);

        if (client_fd == -1){
            perror("accept");
            continue;
        }

        // Get IP string of client's address
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
        
        printf("New client detected!\n");

        while (1) {

            //Receive request from client
            if ((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                perror("server recv");
                return 1;
            }

            //Force the termination of the request string
            buf[numbytes] = '\0'; 
        
            // Print request from client
            printf("Received request: %s\n", buf); //CHANGE BECAUSE THIS IS CAUSING INFINITE PRINT

            // Clear contents of response_headers buffers
            memset(response_headers, 0, sizeof(response_headers));

            // Extract the command
            char *command;
            command = strtok(buf, " ");

            char *arg;
            arg = strtok(NULL, "\r\n");

            


            //If the command is FIND
            if (strcmp(command, "FIND") == 0) {

                int matched_files;

                // Will store the files that match the pattern
                char *ptr_arr[num_lines];

                // Initialize all points to NULL
                for (int i = 0; i < num_lines; i++) {
                    ptr_arr[i] = NULL;
                }

                //Search index for all files that match that pattern
                matched_files = find_files(arg, num_lines, ptr_arr);

                if (matched_files == -1) {
                    printf("Error: failure to find all files");
                    return 1;
                }
       
                // Send response_headers
                sprintf(response_headers, "200 OK\nSearch-Pattern: %s\nMatched-Files: %d\n\n", arg, matched_files); 

                if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                    perror("server: send");
                } 

                // Send matched files 
                for (int i = 0; i < matched_files; i++) {
                    if (ptr_arr[i]) {
                        if (send(client_fd, ptr_arr[i], strlen(ptr_arr[i]), 0) == -1) {
                            perror("server: send");
                        } 
                    }
                }

                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
            }
        


            /*
            // If the command is "HELO"
            if (strstr(buf, "HELO\n") == 0){
                printf("200 OK\nPort: %s\nContent-Directory: %s\nIndexed-Files: %d\n", argv[1], argv[2], num_lines);
            }
            else {
                break;
                printf("400 Error\n");
            }*/
        }
        
        printf("Client connection finished!\n");
        close(client_fd);
  
        memset(s, 0, sizeof(s));
        memset(buf, 0, sizeof(buf));
    }

    return 0;
    
}
