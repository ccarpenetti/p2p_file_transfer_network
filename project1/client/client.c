#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../proto/base64/base64.h"
#include <arpa/inet.h>

#define MAXDATASIZE 256
int client_fd; // socket of client
FILE *decoded_file = NULL;
FILE *encoded_file = NULL;

// Create all subdirectories from the downloaded content
int create_dirs(char *path) {

    // Copy over path
    char temp[MAXDATASIZE];
    strcpy(temp, path);

    // Find the first occurence of a slash (i.e. the first directory)
    char *p = strchr(temp, '/');  

    while (p != NULL) {
        *p = '\0';  // Temporarily split to create first directory

        if (mkdir(temp, 0777) && errno != EEXIST) {
            perror("mkdir");
            return 1;
        }

        *p = '/';

        // Move the pointer to the next directory
        p = strchr(p + 1, '/');
    }

    return 0;

}

// Iterate through port argument to see if it contains all digits
int check_port(char *port){
    
    while (*port) {

        if (isdigit(*port) == false) {
            return 1;
        }
        port++;
    }

    return 0;
}

// Get socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {

    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        // Establish timeout (using setting the socket options)
        if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt");
            close(client_fd);
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
    if (argc != 4) {
        printf("Error: incorrect number of arguments.\n");
        return 1;
    }


    char *server_ip = argv[1];
    char *server_port = argv[2];
    char *dl_dir = argv[3];

    // Check that the user entered a valid port value (i.e. a number)
    if (check_port(server_port) == 1) { 
        printf("Error: invalid port number.\n");
        return 1;
    }

    // Setup client
    if (setup_client(server_ip, server_port) == 1){
        return 1;
    }

    int numbytes;
    int end = false; //bool about when to end

    // Will store user input from command line
    char input[MAXDATASIZE];
    memset(input, 0, sizeof(input));

    // Will store server response
    char response[MAXDATASIZE];
    memset(response, 0, sizeof(MAXDATASIZE));

    // Wait for stdin
    while (1) {

        // Gather stdin
        fgets(input, MAXDATASIZE, stdin);

        // Remove the newline from stdin
        input[strlen(input) - 1] = '\0';

        // Signal that we should end communcation with server
        if (strcmp(input, "END") == 0) {
            end = true;
        }

        // Add a CR/LF
        strcat(input, "\r\n");

        //Send message to server      
        if (send(client_fd, input, strlen(input), 0) == -1) {
            perror("client send");
        }
        
        memset(input, 0, sizeof(input));

        char path[MAXDATASIZE]; // path for creating files from GETR

        printf("Received a response:\n");
        memset(response, 0, sizeof(response));
        // Receive response from server
        while ((numbytes = recv(client_fd, response, MAXDATASIZE - 1, 0)) > 0) {

            //Force the termination of the string
            response[numbytes] = '\0'; 
            
            printf("%s", response);
           
            //For GETR response, extract the name of the file to be created
            char *file_name = strstr(response, "File-Name:");
            
            if (file_name != NULL) {
                file_name += strlen("File-Name: ");
                file_name = strtok(file_name, "\n");
                sprintf(path, "%s/%s", dl_dir, file_name);

                //Create directory structure 
                if (create_dirs(path) == 1) {
                    perror("Can't create directory structure");
                    return 1;
                }

                // Open files for encoded data and decoded data
                decoded_file = fopen(path, "w");
                encoded_file = fopen("encoded.txt", "w+");
                if (!decoded_file || !encoded_file) {
                    perror("cant open file");
                    return 1;
                }
            }

            /* With how my code is set up, the node's response headers will always be sent in a separate TCP message
            so, to retrieve only the encoded data, I ensure that the current node's response does not have a status code */
            char *h1 = strstr(response, "200 OK");
            char *h2 = strstr(response, "400 Error");
    
            if (!h1 && !h2 && (encoded_file != NULL)){
                // If we've encountered encoded data, write it to encoded file
                fwrite(response, sizeof(char), numbytes, encoded_file);
            }
 
            if (strstr(response, "\r\n") != NULL) {
                break; // Exit loop if end of response
            }

            memset(response, 0, sizeof(response));
        }

        // Decode file, if needed
        if (decoded_file != NULL && encoded_file != NULL){
            fseek(encoded_file, 0, SEEK_SET);
            decode(encoded_file, decoded_file);
            fclose(encoded_file);
            fclose(decoded_file);
            encoded_file = NULL;
            decoded_file = NULL;
        }
    
        if (numbytes == -1) {
            perror("client recv");
            return 1;
        }

        // If client wants to end session, break from stdin loop
        if (end) {
            break;
        }

    }    

    // Close client socket
    close(client_fd);

    return 0;
} 
