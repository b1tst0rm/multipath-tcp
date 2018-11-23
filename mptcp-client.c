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
#include <wait.h>


#define CTL_PORT 50000    // Control Port for DSN
#define DATA_PORT_1 50001 // Data subflows 1-3
#define DATA_PORT_2 50002
#define DATA_PORT_3 50003
#define SERV_IP "127.0.0.1"


int port_index = 0; // global variable

int subflow_index = 0;

// FUNCTION PROTOTYPES
int create_subflow(int port);
void subflow(int pipe_fd[2], int sf_fd);


int main (int argc, char const *argv[])
{
        char base_data[] = "0123456789"
                           "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int ctl_fd, ctl_retval, i, child_pid;
        char buffer[1024] = {0};
        char message[] = "Test temporary message.";
        char serv_ip_addr[] = "127.0.0.1";
        unsigned short sf_ports[3] = {DATA_PORT_1, DATA_PORT_2, DATA_PORT_3};
        int pipe_fd[2]; // Array of file descriptors for pipe 

        ctl_fd = create_subflow(CTL_PORT);

        send(ctl_fd, message, strlen(message), 0);
        ctl_retval = read(ctl_fd, buffer, 1024);

        printf("Response from server: %s\n", buffer);

        // Create pipe for process-to-process communication
        if (pipe(pipe_fd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
        }

        int sf_fd1 = 1;//create_subflow(DATA_PORT_1);
        int sf_fd2 = 2;//create_subflow(DATA_PORT_2);
        int sf_fd3 = 3;//create_subflow(DATA_PORT_3);


        int fork1 = fork();
        int fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                // child process
                subflow_index++;
                if (subflow_index == 1) {
                        subflow(pipe_fd, sf_fd1);
                } else if (subflow_index == 2) {
                        subflow(pipe_fd, sf_fd2);
                } else {
                        subflow(pipe_fd, sf_fd3); 
                }
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else {
                close(pipe_fd[0]); // close input to write
                write(pipe_fd[1], "one1", 4);
                write(pipe_fd[1], "two2", 4);
                write(pipe_fd[1], "thr3", 4);
                int wpid, status=0;
                // Wait until all child processes finish executing
                while ((wpid = wait(&status)) > 0) {}
                printf("Ending program\n"); 
                close(ctl_fd); // End control connection to server
        }
    
        return EXIT_SUCCESS;
}


int create_subflow(int port) {
        int sf_fd, enable, sf_retval, i;
        struct sockaddr_in serv_addr;
        char buffer[1024] = {0};
        char serv_ip_addr[] = SERV_IP;

        // Set up the control flow socket
        if ((sf_fd = socket(AF_INET, SOCK_STREAM /* TCP */, 0 /* IP proto */)) < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }

        // Setup socket to be reusable
        enable = 1;
        if (setsockopt(sf_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }

        // Set up server information
        memset(&serv_addr, '0', sizeof(serv_addr)); // allocate memory to store address
        serv_addr.sin_port = htons(port); // network byte order conversion
        serv_addr.sin_family = AF_INET; // IPv4

        // convert IP address from text to binary
        if (inet_pton(AF_INET, serv_ip_addr, &serv_addr.sin_addr) <= 0) {
                perror("inet_pton");
                exit(EXIT_FAILURE);
        }

        if (connect(sf_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("connect");
                exit(EXIT_FAILURE);
        }

        return sf_fd; // returns the file descriptor of a connected socket 
}


// Child processes call this to execute MP subflow code
void subflow(int pipe_fd[2], int sf_fd) {
        printf("in subflow as child %d\n", getpid());
        printf("sf_fd = %d", sf_fd);
        fflush(stdout);
        close(pipe_fd[1]);
        char buf[4];
        read(pipe_fd[0], buf, 4);
        printf("%s\n", buf);

//
//        send(sf_fd, message, strlen(message), 0);
//        
//        close(sf_fd); // End control connection to server
}
