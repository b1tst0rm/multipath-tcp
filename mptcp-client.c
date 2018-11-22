/********************************
* MPTCP Client (mptcpclient.c)  *
* Client file for multipath TCP *
* Author: Daniel Limanowski     *
********************************/
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>


#define CTL_PORT 50000    // Control Port for DSN
#define DATA_PORT_1 50001 // Data subflows 1-3
#define DATA_PORT_2 50002
#define DATA_PORT_3 50003

int port_index = 0; // global variable

// FUNCTION PROTOTYPES
void subflow(int pipe_fd[2]);


int main (int argc, char const *argv[])
{
        char base_data[] = "0123456789"
                           "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int ctl_fd, enable, ctl_retval, i;
        struct sockaddr_in serv_addr;
        char buffer[1024] = {0};
        char message[] = "Test temporary message.";
        char serv_ip_addr[] = "127.0.0.1";
        pid_t subflow_procs[2];
        unsigned short sf_ports[3] = {DATA_PORT_1, DATA_PORT_2, DATA_PORT_3};
        int pipe_fd[2]; // Array of file descriptors for pipe 

        // Set up the control flow socket
        if ((ctl_fd = socket(AF_INET, SOCK_STREAM /* TCP */, 0 /* IP proto */)) < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }

        // Setup socket to be reusable
        enable = 1;
        if (setsockopt(ctl_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }

        // Set up server information
        memset(&serv_addr, '0', sizeof(serv_addr)); // allocate memory to store address
        serv_addr.sin_port = htons(CTL_PORT); // network byte order conversion
        serv_addr.sin_family = AF_INET; // IPv4

        // convert IP address from text to binary
        if (inet_pton(AF_INET, serv_ip_addr, &serv_addr.sin_addr) <= 0) {
                perror("inet_pton");
                exit(EXIT_FAILURE);
        }

        if (connect(ctl_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("connect");
                exit(EXIT_FAILURE);
        }

        send(ctl_fd, message, strlen(message), 0);
        ctl_retval = read(ctl_fd, buffer, 1024);

        printf("Response from server: %s\n", buffer);
        
        close(ctl_fd); // End control connection to server

        if (pipe(pipe_fd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
        }

        // Need to call fork() 2x such that there are 4 total processes (2^2 = 4)
        // 1 control process and 3 subflow processes
        for (i = 0; i < 2; i++) {
                if ((subflow_procs[i] = fork()) < 0) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                } else if (subflow_procs[i] == 0) {
                        // Child in here
                        subflow(pipe_fd);
                } else {
                        // Parent here
                        close(fd[0]); // close input to write
                        write(fd[1], "pipelol", 7);

                }
        }

        return EXIT_SUCCESS;
}

// Child processes call this to execute MP subflow code
void subflow(int pipe_fd[2]) {
        port_index++;
        printf("Port %d\n", port_index);
//        int sf_fd, enable, sf_retval, i;
//        struct sockaddr_in serv_addr;
//        char buffer[1024] = {0};
//        char message[] = "Test temporary message.";
//        char serv_ip_addr[] = "127.0.0.1";
//
//        // Set up the control flow socket
//        if ((sf_fd = socket(AF_INET, SOCK_STREAM /* TCP */, 0 /* IP proto */)) < 0) {
//                perror("socket");
//                exit(EXIT_FAILURE);
//        }
//
//        // Setup socket to be reusable
//        enable = 1;
//        if (setsockopt(sf_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
//                perror("setsockopt");
//                exit(EXIT_FAILURE);
//        }
//
//        // Set up server information
//        memset(&serv_addr, '0', sizeof(serv_addr)); // allocate memory to store address
//        serv_addr.sin_port = htons(port); // network byte order conversion
//        serv_addr.sin_family = AF_INET; // IPv4
//
//        // convert IP address from text to binary
//        if (inet_pton(AF_INET, serv_ip_addr, &serv_addr.sin_addr) <= 0) {
//                perror("inet_pton");
//                exit(EXIT_FAILURE);
//        }
//
//        if (connect(sf_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//                perror("connect");
//                exit(EXIT_FAILURE);
//        }
//
//        send(sf_fd, message, strlen(message), 0);
//        
//        close(sf_fd); // End control connection to server
}
