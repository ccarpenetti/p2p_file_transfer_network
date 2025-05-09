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

#define BACKLOG 10
#define MAXDATASIZE 100

int server_fd;

// Handler for CTRL-C
void sig_handler(int s) {

    printf("Control-C detected - shutting down server\n");
    close(server_fd);
    exit(0);

}

bool check_port(char *port){
    
    // Iterate through port argument to see if it contains all digits
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

//Invert string
char *inverse_string(char *str){

    int len = strlen(str);
    char new_str[len];
    int i = 0;

    while (len > 0) {
        new_str[i] = str[len - 1];
        i++;
        len--;
    }

    new_str[i] = '\0';

    strcpy(str, new_str);
    return str;

}

int main(int argc, char *argv[]){

    // Check for correct number of arguments
    if (argc != 2) {
        printf("Error: incorrect number of arguments.\n");
        return 0;
    }

    //Check that the user entered a valid port value (i.e. a number)
    if (check_port(argv[1]) == false) {
        printf("Error: invalid port number.\n");
        return 1;
    }

    int server_fd, client_fd; // Will be listening on server_fd, new connection with client_fd

    int rv, numbytes;
    int yes = 1;

    struct addrinfo hints, *servinfo, *p; 
    struct sockaddr_storage their_addr; // hold's the client's address information

    char s[INET6_ADDRSTRLEN]; // store string form of address
    memset(s, 0, sizeof(s));
  
    char buf[MAXDATASIZE];
    memset(buf, 0, sizeof(buf));
    
    // will hold our response messages
    char code[MAXDATASIZE];        
    char content_length[MAXDATASIZE];        
    char inversion[MAXDATASIZE];   
        
 
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
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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

        //Receive request from client
        if ((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
            perror("server recv");
            return 1;
        }

        //Force the termination of the request string
        buf[numbytes] = '\0'; 
    
        // Print request from client
        printf("%s\n", buf);

        // Clear contents of response buffers
        memset(&code, 0, sizeof(code));
        memset(&content_length, 0, sizeof(content_length));
        memset(&inversion, 0, sizeof(inversion));

        // If the command is anything but INVERT
        if (!strstr(buf, "INVERT")){
            //Send error message to client
            strcpy(code, "501 NOT IMPLEMENTED");
            if (send(client_fd, code, strlen(code), 0) == -1) {
                perror("server: send");
            }
        }
        else {
            
            strcpy(code, "200 OK");
            
            char *word;
            word = strchr(buf, ' ');
            word++;
            
            sprintf(content_length, "Content-Length: %d", strlen(word));
            sprintf(inversion, "Inversion: %s", inverse_string(word));

            if (send(client_fd, code, strlen(code), 0) == -1) {
                perror("server: send");
            }
            
            printf("Sent: %s\n", code);

            if (send(client_fd, content_length, strlen(content_length), 0) == -1) {
                perror("server: send");
            }
            
            printf("Sent: %s\n", content_length);

            if (send(client_fd, inversion, strlen(inversion), 0) == -1) {
                perror("server: send");
            }
            
            printf("Sent: %s\n", inversion);

            

        }

        printf("Client connection finished!\n");
        close(client_fd);
  
        memset(s, 0, sizeof(s));
        memset(buf, 0, sizeof(buf));
    }

    return 0;
    
}
