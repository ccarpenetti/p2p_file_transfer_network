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
#include "../proto/index/index.h"
#include "../proto/base64/base64.h"
#include <fnmatch.h>

#define BACKLOG 10
#define MAXDATASIZE 256

// Structure that will be used to store our list of nodes
typedef struct {

    char id[100];
    char ip[100];
    char port[100];
    char file_num[100];
    char exp[100];
    
} Node;

// Globals
int server_fd; //Server socket
FILE *index_file;
Node *list[10];
int node_id; // your node id given to you by the tracker
int neighbor_fd; //SOcket to communicate to other node

// Handler for CTRL-C
void sig_handler(int s) {

    printf("Control-C detected - shutting down server\n");
    close(server_fd);
    exit(0);

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

// Parse the request received from the client
void parse_req(char *buf, char **command, char **param_1, char **param_2){

    char *remove_white;
    remove_white = strtok(buf, "\r\n"); //Remove CR LF
            
    *command = strtok(remove_white, " "); // Parse to retrieve commands

    *param_1 = strtok(NULL, " "); // Gather parameters if needed
    *param_2 = strtok(NULL, " "); // Gather parameters if needed
    
}

// Register node with the tracker
int register_node(char *port, int num_lines, char *tracker_ip, char *tracker_port){

    //Construct message
    uint8_t message[12] = {0};

    message[0] = 0x01; // Type
    message[1] = 0x00; // Length
    message[2] = 0x0C;
    message[3] = 0x00;  // Identifier - new registration
    inet_pton(AF_INET, "127.0.0.1", &message[4]); // IP Address

    // Convert the port number
    uint16_t num_port;
    sscanf(port, "%hu", &num_port);
    uint16_t net_port = htons(num_port);
    memcpy(&message[8], &net_port, sizeof(uint16_t)); // Port Number

    // Convert number of files
    uint16_t net_num_lines = htons(num_lines); 
    memcpy(&message[10], &net_num_lines, sizeof(uint16_t)); // Num Files

    // Create UDP socket
    int tracker_sock;
    struct sockaddr_in tracker_addr;

    if ((tracker_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("tracker socket creation failed");
        return 1;
    }

    memset(&tracker_addr, 0, sizeof(tracker_addr));
    tracker_addr.sin_addr.s_addr = inet_addr(tracker_ip); 
    tracker_addr.sin_family = AF_INET;

    uint16_t net_tracker_port;
    sscanf(tracker_port, "%hu", &net_tracker_port);
    tracker_addr.sin_port = htons(net_tracker_port);

    // Send the message
    if (sendto(tracker_sock, message, sizeof(message), 0, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0) {
        perror("tracker send failed");
        close(tracker_sock);
        return 1;
    }

    char buf[18];
    memset(buf, 0, sizeof(buf));
    socklen_t length = sizeof(tracker_addr);
    
    // Receive tracker response
    int numbytes = 0;
    if ((numbytes = recvfrom(tracker_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&tracker_addr, &length)) < 0){
        perror("server recv");
        close(tracker_sock);
        return 1;
    }

    buf[numbytes] = '\0';  // Forcefully terminate the string

    printf("Received Register-ACK from tracker:");

    for (int i = 0; i < numbytes; i++){
        printf(" %02x", (unsigned char)buf[i]);    
    }

    printf("\n");

    // Get identifier
    node_id = (unsigned char)buf[4];
    printf("Node's Tracker ID: %d\n", node_id);

    close(tracker_sock);
    return 0;

}

// List-nodes request to tracker
int list_nodes(char *tracker_ip, char *tracker_port){

    // Construct message
    uint8_t message[4] = {0};

    message[0] = 0x03; // Type
    message[1] = 0x00; // Length
    message[2] = 0x04; 
    message[3] = 0x0A; // MaxCount
    
    // Create UDP socket
    int tracker_sock;
    struct sockaddr_in tracker_addr;

    if ((tracker_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket to tracker creation failed");
        return 1;
    }

    memset(&tracker_addr, 0, sizeof(tracker_addr));
    tracker_addr.sin_addr.s_addr = inet_addr(tracker_ip); 
    tracker_addr.sin_family = AF_INET;

    uint16_t net_tracker_port;
    sscanf(tracker_port, "%hu", &net_tracker_port);
    net_tracker_port = htons(net_tracker_port);
    tracker_addr.sin_port = net_tracker_port;

    // Send the message
    if (sendto(tracker_sock, message, sizeof(message), 0, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0) {
        perror("tracker send failed");
        close(tracker_sock);
        return 1;
    }
    
    char buf[MAXDATASIZE];
    memset(buf, 0, sizeof(buf));
    socklen_t length = sizeof(tracker_addr);

    // Receive tracker response
    int numbytes = 0;
    if ((numbytes = recvfrom(tracker_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&tracker_addr, &length)) < 0){
        perror("server recv");
        close(tracker_sock);
        return 1;
    }

    buf[numbytes] = '\0';  // Forcefully terminate the string

    printf("Received List-Nodes-Data from tracker:");

    for (int i = 0; i < numbytes; i++){
        printf(" %02x", (unsigned char)buf[i]);    
    }

    printf("\n");

    // Using refreshed list, clear current list of nodes and make a new list
    memset(list, 0, sizeof(list));
    int index = 0;

    // Add nodes to the array of Nodes
    for (int i = 6; i < numbytes; i += 13){
        Node *n = (Node *)malloc(sizeof(Node));

        sprintf(n->id, "%u", (unsigned char)buf[i]); 
        sprintf(n->ip, "%u.%u.%u.%u", (unsigned char)buf[i+1], (unsigned char)buf[i+2], (unsigned char)buf[i+3], (unsigned char)buf[i+4]);
        sprintf(n->port, "%u",(((unsigned char)buf[i+5] << 8) | (unsigned char)buf[i+6]));
        sprintf(n->file_num, "%u", (((unsigned char)buf[i+7] << 8) | (unsigned char)buf[i+8]));
        sprintf(n->exp, "%u", ((unsigned char)buf[i+9] << 24) | ((unsigned char)buf[i+10] << 16) | ((unsigned char)buf[i+11] << 8) | ((unsigned char)buf[i+12]));
    
        list[index++] = n;

    }
    
    close(tracker_sock);
    return 0;

}

// Given the pattern to match, find all files that match that pattern
int find_files(char *pattern, int num_lines, char *ptr_arr[]) {

    int i = 0;

    // Create the full pattern
    char full_pattern[MAXDATASIZE];
    sprintf(full_pattern, "*;%s;*", pattern);

    // If we read the file previously, set the file pointer back to the start of file
    fseek(index_file, 0, SEEK_SET);

    char line[MAXDATASIZE];

    // Iterate through all of file to see which files match the pattern
    while (fgets(line, sizeof(line), index_file)){
        if (fnmatch(full_pattern, line, 0) == 0) {
            ptr_arr[i] = strdup(line);
            if (ptr_arr[i] == NULL) {
                perror("strdup failed");
                return -1;
            }
            i++;
        }   
    }
    
    // Return the number of matched files
    return i;

}

// GET request for a file
int get_file(int file_num, int num_lines, char *response_headers, char *index_dir){

    // If we read the file previously, set the file pointer back to the start of file
    fseek(index_file, 0, SEEK_SET);

    // Iterate through all of file to see which file has that index
    char line[MAXDATASIZE];

    int i = 1;
    while (fgets(line, sizeof(line), index_file)){
        if (i == file_num) {
            break;
        }
        i++;
    }
   
    // Parse file entry to create headers 
    char *num = strtok(line, ";");
    char *name = strtok(NULL, ";");
    char *size = strtok(NULL, ";");
    char *date = strtok(NULL, ";");
    int enc_size;

    // Create full path of file
    char path[MAXDATASIZE];
    sprintf(path, "%s/%s", index_dir, name);
    
    // Open up file
    FILE *f_input = fopen(path, "r");
    FILE *f_output = fopen("encoded.txt", "w");
    if (!f_input || !f_output) {
        perror("Error opening file");
        return 1;
    }  

    // Encode file
    encode(f_input, f_output, &enc_size);

    // Write headers
    sprintf(response_headers, "200 OK\nRequest-File: %s\nFile-Name: %s\nFile-Size: %s\nFile-Date: %sEncoded-Size: %d\n\n", num, name, size, date, enc_size);

    fclose(f_input);
    fclose(f_output);

    return 0;

}

// Get socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {

    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Set up connection with neighboring node
// Setup the client and connect to the server
int setup_neighbor(char *host, char *port) {

    struct addrinfo hints, *servinfo, *p;

    int rv;

    char s[INET6_ADDRSTRLEN]; // store string form of address
    memset(s, 0, sizeof(s));
  
    // Initialize specifications for communication (TCP)
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
        if ((neighbor_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        // Establish timeout (using setting the socket options)
        if (setsockopt(neighbor_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt");
            close(neighbor_fd);
            continue;
        }

        // Upon successful socket creation, establish 3-way handshake
        if (connect(neighbor_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: connect");
            close(neighbor_fd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to connect to neighbor\n");
        return 1;
    }

    //Put IP address in string format
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));

    printf("Connected successfully to neighbor %s:%s\n", s, port);

    freeaddrinfo(servinfo);

    return 0;

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

int main(int argc, char *argv[]){

    // Check for correct number of arguments
    if (argc != 5) {
        fprintf(stderr, "Error: incorrect number of arguments.\n");
        return 1;
    }

    char *port  = argv[1];
    char *index_dir = argv[2];
    char *tracker_ip = argv[3];
    char *tracker_port = argv[4];

    //Check that the user entered valid port values (i.e. a number)
    if ((check_port(port) == 1) || (check_port(tracker_port)) == 1) {

        fprintf(stderr, "Error: invalid port number.\n");
        return 1;
    }

    // Index the specified directory
    int num_lines = 0;
    index_file = fopen("index.txt", "w+");

    if (index_file == NULL){
        perror("Error opening file");
        return 1;
    }
    
    num_lines = get_index(index_dir, strlen(index_dir), index_file);

    if (num_lines == -1) {
        fprintf(stderr, "Error: could not create index.\n");
        return 1;
    }
    
    // Register with the tracker by sending registration message
    if (register_node(port, num_lines, tracker_ip, tracker_port) == 1) {
        perror("Node registration fail");
        return 1;
    }

    // Setup server
    if (setup_server(port) == 1) {
        return 1;
    }
    
    printf("Server successfully started listening on Port %s\n", port);

    // Initialize variables
    int numbytes;

    int client_fd; //New connection with client_fd
    struct sockaddr_storage their_addr; // holds the client's address information
    
    char buf[MAXDATASIZE]; // will hold incoming messages
    memset(buf, 0, sizeof(buf));

    char s[INET6_ADDRSTRLEN]; // store string form of address
    memset(s, 0, sizeof(s));

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

        // Fulfilling the requests of one client (sequential server)
        while (1) {
    
            //Receive request from client
            if ((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                perror("server recv");
                return 1;
            }

            //Force the termination of the request string
            buf[numbytes] = '\0'; 
        
            // Print request from client
            printf("Received request: %s\n", buf);

            // Parse request
            char *command = NULL;
            char *param_1 = NULL;
            char *param_2 = NULL;  
            parse_req(buf, &command, &param_1, &param_2);   

            // Will hold different parts of the node response
            char response_headers[MAXDATASIZE]; 
            char response_content[MAXDATASIZE];
                       

            // If the command is HELO
            if (strcmp(command, "HELO") == 0) {
                // If client entered HELO command correctly (with no extra params)
                if (!param_1 && !param_2) {
                    // Send response
                    sprintf(response_headers, "200 OK\nPort: %s\nContent-Directory: %s\nIndexed-Files: %d\n", port, index_dir, num_lines);
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                } 
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: HELO does not take any parameters\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
               
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 

            }

            // If the command is REFRESHR
            else if (strcmp(command, "REFRESHR") == 0) {

                // If no params were entered (i.e. the correct way to call REFRESHR)
                if (!param_1 && !param_2) {
                    // Send List-Nodes message to tracker
                    if (list_nodes(tracker_ip, tracker_port) == 1){
                        perror("REFRESHR fail");
                        return 1;
                    }
                    
                    sprintf(response_headers, "200 OK\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                    int i = 0;

                    // Send list of refreshed nodes to client 
                    while (list[i]) {
                        sprintf(response_content, "%s;%s;%s;%s;%s\n", list[i]->id, list[i]->ip, list[i]->port, list[i]->file_num, list[i]->exp);
                        if (send(client_fd, response_content, strlen(response_content), 0) == -1) {
                            perror("server: send");
                        } 
                        i++;
                    }                
                }
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: REFRESHR does not take any parameters\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
                
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
                

            }
            // If the command is FIND
            else if (strcmp(command, "FIND") == 0){
                // We need one param for FIND
                if (param_1 && !param_2){
                    int matched_files;
                    // Will store the file entries that match the pattern
                    char *ptr_arr[num_lines];
                    for (int i = 0; i < num_lines; i++) {
                        ptr_arr[i] = NULL;
                    }

                    //Search index for all files that match that pattern
                    matched_files = find_files(param_1, num_lines, ptr_arr);

                    if (matched_files == -1) {
                        printf("Error: failure to find all files");
                        return 1;
                    }

                    // Send response_headers
                    sprintf(response_headers, "200 OK\nSearch-Pattern: %s\nMatched-Files: %d\n\n", param_1, matched_files);

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
                }
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: FIND takes one parameter\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
            }
            // If the command is FINDR
            else if (strcmp(command, "FINDR") == 0){
                
                // Two params need to be entered for FINDR
                if (param_1 && param_2) {
                    // If the node would like to conduct a FIND on our node
                    if (atoi(param_1) == node_id){
                        int matched_files;
                        // Will store the file entries that match the pattern
                        char *ptr_arr[num_lines];
                        for (int i = 0; i < num_lines; i++) {
                            ptr_arr[i] = NULL;
                        }

                        //Search index for all files that match that pattern
                        matched_files = find_files(param_2, num_lines, ptr_arr);

                        if (matched_files == -1) {
                            printf("Error: failure to find all files");
                            return 1;
                        }

                        // Send response_headers
                        sprintf(response_headers, "200 OK\nSearch-Pattern: %s\nMatched-Files: %d\n\n", param_2, matched_files);

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
                    }
                    // Else, they are requesting the information from another node
                    else {
                        // Forward TCP message from client to the desired node
                        char forward_msg[MAXDATASIZE];
                        memset(forward_msg, 0, MAXDATASIZE);

                        sprintf(forward_msg, "%s %s %s\r\n", command, param_1, param_2);
                        
                        // Check node list for desired node
                        int i = 0;
                        int found_node = 0;
                        while (list[i]) {
                            if (strcmp(list[i]->id, param_1) == 0) {
                                printf("Found node: %s %s %s\n", list[i]->id, list[i]->ip, list[i]->port);
                                found_node = 1;

                                //Establish TCP connection with this node
                                if (setup_neighbor(list[i]->ip, list[i]->port) == 1){
                                    return 1;
                                }

                                if (send(neighbor_fd, forward_msg, strlen(forward_msg), 0) == -1) {
                                    perror("server: send");
                                }

                
                                while (1) {
                                    memset(buf, 0, sizeof(buf));
                                    //Receive request from neighbor
                                    if ((numbytes = recv(neighbor_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                                        perror("server recv");
                                        return 1;
                                    }
                   
                                    if (strstr(buf, "\r\n")  != NULL) {
                                        break; // Exit loop if end of response
                                    }       
                                    // Pass it on to the client
                                    if (send(client_fd, buf, strlen(buf), 0) == -1) {
                                        perror("server: send");
                                    }

                                }
                                
                                // Shutdown the connection with neighbor by sending "END"
                                if (send(neighbor_fd, "END\r\n", strlen("END\r\n"), 0) == -1) {
                                    perror("server: send");
                                } 
                                //Receive request from neighbor
                                if ((numbytes = recv(neighbor_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                                    perror("server recv");
                                    return 1;
                                }
                                printf("Closed down connection to neighbor.\n");

                                close(neighbor_fd); 
                                break;
                            }
                            i++;
                        }
                   } 
                }
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: FINDR takes two parameters\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
            }
            // If the command is GET
            else if (strcmp(command, "GET") == 0) {
                if (param_1 && !param_2) { // Only one parameter needed for GET

                    // If the user enters a file index that does not exist
                    if (atoi(param_1) > num_lines) {
                        sprintf(response_headers, "400 Error: file index does not exist\n");
                        if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                            perror("server: send");
                            return 1;
                        } 
                    } 
                        
                    // Perform normal GET request                    
                    if (get_file(atoi(param_1), num_lines, response_headers, index_dir) == 1){
                        return 1;
                    }

                    // Send headers
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 

                    // Take encoded data (i.e. reading from the encoded.txt file) and send to client
                    FILE *encoded_file = fopen("encoded.txt", "r");
                    if (!encoded_file) {
                        perror("Error opening file");
                        return 1;
                    } 

                    char line[MAXDATASIZE];

                    while (fgets(line, sizeof(line), encoded_file)){
                        if (send(client_fd, line, strlen(line), 0) == -1) {
                            perror("server: send");
                        } 
                    }

                }
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: GET takes one parameter\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
            }
            // If the command is GETR
            else if (strcmp(command, "GETR") == 0) {
            
                // Two params need to be entered for GETR
                if (param_1 && param_2) {

                    // If the user enters a file index that does not exist
                    if (atoi(param_2) > num_lines) {
                        sprintf(response_headers, "400 Error: file index does not exist\n");
                        if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                            perror("server: send");
                        } 
                    } 
                    // See if client is requesting information from our own node 
                    else if (atoi(param_1) == node_id) {
                        
                        // Perform normal GET request                    
                        if (get_file(atoi(param_2), num_lines, response_headers, index_dir) == 1){
                            return 1;
                        }

                        // Send headers
                        if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                            perror("server: send");
                        } 

                        // Take encoded data (i.e. reading from the encoded.txt file) and send to client
                        FILE *encoded_file = fopen("encoded.txt", "r");
                        if (!encoded_file) {
                            perror("Error opening file");
                            return 1;
                        } 
 
                        char line[MAXDATASIZE];

                        while (fgets(line, sizeof(line), encoded_file)){
                            if (send(client_fd, line, strlen(line), 0) == -1) {
                                perror("server: send");
                            } 
                        }

                    }
                    else { // They want information from another node 
                        // Forward TCP message from client to the desired node
                        char forward_msg[MAXDATASIZE];
                        memset(forward_msg, 0, MAXDATASIZE);

                        sprintf(forward_msg, "%s %s %s\r\n", command, param_1, param_2);
                        
                        // Check node list for desired node
                        int i = 0;
                        int found_node = 0;
                        while (list[i]) {
                            if (strcmp(list[i]->id, param_1) == 0) {
                                printf("Found node: %s %s %s\n", list[i]->id, list[i]->ip, list[i]->port);
                                found_node = 1;

                                //Establish TCP connection with this node
                                if (setup_neighbor(list[i]->ip, list[i]->port) == 1){
                                    return 1;
                                }

                                if (send(neighbor_fd, forward_msg, strlen(forward_msg), 0) == -1) {
                                    perror("server: send");
                                }

                
                                while (1) {
                                    memset(buf, 0, sizeof(buf));
                                    //Receive request from neighbor
                                    if ((numbytes = recv(neighbor_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                                        perror("server recv");
                                        return 1;
                                    }

                                    if (strstr(buf, "\r\n")  != NULL) {
                                        break; // Exit loop if end of response
                                    }       
                   
                                    // Pass it on to the client
                                    if (send(client_fd, buf, strlen(buf), 0) == -1) {
                                        perror("server: send");
                                    }

                                }

                                // Shutdown the connection with neighbor by sending "END"
                                if (send(neighbor_fd, "END\r\n", strlen("END\r\n"), 0) == -1) {
                                    perror("server: send");
                                } 
                                //Receive request from neighbor
                                if ((numbytes = recv(neighbor_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                                    perror("server recv");
                                    return 1;
                                }

                                close(neighbor_fd);
                                break;
                            }
                            i++;
                        }
                        if (found_node == 0){
                            // Could not find node with that id
                            sprintf(response_headers, "400 Error: no node with that identifier\n");
                            if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                                perror("server: send");
                            } 
    
                            // Send final blank line
                            if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                                perror("server: send");
                            } 
                        }     
                    }
                }
                // Else, send appropriate error message
                else {
                    sprintf(response_headers, "400 Error: GETR takes two parameters\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 

            }

            // If the command if END
            else if (strcmp(command, "END") == 0) {
                // If client entered END command correctly (with no extra params)
                if (!param_1 && !param_2) {
                    // Send headers
                    sprintf(response_headers, "200 OK\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                    // Send final blank line
                    if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                        perror("server: send");
                    } 
                    break; // done with client, break from loop  

                } // Else send error message
                else {
                    sprintf(response_headers, "400 Error: END does not take any parameters\n");
                    if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                        perror("server: send");
                    } 
                }

                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
                
            } 
            // If the command is not a valid command
            else {
                sprintf(response_headers, "400 Error: Invalid Command\n");
                if (send(client_fd, response_headers, strlen(response_headers), 0) == -1) {
                    perror("server: send");
                } 
                
                // Send final blank line
                if (send(client_fd, "\r\n", strlen("\r\n"), 0) == -1) {
                    perror("server: send");
                } 
            }

            memset(buf, 0, sizeof(buf));

        }
    
        printf("Client connection finished!\n");
        close(client_fd);
  
        memset(s, 0, sizeof(s));
    }


    return 0;
    
}
