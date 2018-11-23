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


// FUNCTION PROTOTYPES
int create_subflow(int port);
void subflow(int read_pipe[2], int write_pipe[2]);


int main (int argc, char const *argv[])
{
        char base_data[] = "0123456789"
                           "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int ctl_fd, ctl_retval, i;
        char buffer[1024] = {0};
        char message[] = "Test temporary message.";
        int parent_pipe[2]; // Parent writes, child reads 
        int child_pipe[2]; // Child writes, parent reads 

        ctl_fd = create_subflow(CTL_PORT);
        send(ctl_fd, message, strlen(message), 0);
        ctl_retval = read(ctl_fd, buffer, 1024);
        printf("Response from server: %s\n", buffer);

        // Create pipes for process-to-process communication
        if (pipe(parent_pipe) < 0) {
                perror("parent pipe");
                exit(EXIT_FAILURE);
        }

        if (pipe(child_pipe) < 0) {
                perror("child pipe");
                exit(EXIT_FAILURE);
        }
        
        int sf_fd1 = create_subflow(DATA_PORT_1);
        int sf_fd2 = create_subflow(DATA_PORT_2);
        int sf_fd3 = create_subflow(DATA_PORT_3);

        int fork1 = fork();
        int fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                // child process
                subflow(parent_pipe, child_pipe);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else {
                // parent process
                char resp1, resp2, resp3;
                close(parent_pipe[0]); // close input to write
                close(child_pipe[1]); // close output to read
                write(parent_pipe[1], &sf_fd1, sizeof(int));
                write(parent_pipe[1], &sf_fd2, sizeof(int));
                write(parent_pipe[1], &sf_fd3, sizeof(int));
                read(child_pipe[0], &resp1, sizeof(char));
                read(child_pipe[0], &resp2, sizeof(char));
                read(child_pipe[0], &resp3, sizeof(char));
                printf("Responses: %c %c %c\n", resp1, resp2, resp3);

                int wpid, status=0;
                // Wait until all child processes finish executing
                while ((wpid = wait(&status)) > 0) {}
                printf("Ending client\n"); 
                close(ctl_fd); // End control connection to server
        }
    
        return EXIT_SUCCESS;
}


int create_subflow(int port)
{
        int sf_fd, enable, sf_retval, i;
        struct sockaddr_in serv_addr;
        char buffer[1024] = {0};
        char serv_ip_addr[] = SERV_IP;

        // Set up the socket
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

        // Connect to the server application
        if (connect(sf_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("connect");
                exit(EXIT_FAILURE);
        }

        // Return the file descriptor of connected socket 
        return sf_fd;
}


// Child processes call this to execute MP subflow code
void subflow(int read_pipe[2], int write_pipe[2])
{
        int serv_fd = 0;
        // First the subflow will read in it's socket file descriptor
        close(read_pipe[1]); // close the write end of the read pipe
        read(read_pipe[0], &serv_fd, sizeof(int));

        // Acknowledge receipt of socket fd by writing Y to pipe (Y = yes)
        char yes = 'Y';
        close(write_pipe[0]);
        write(write_pipe[1], &yes, sizeof(char));
        printf("child %d got %d\n", getpid(), serv_fd);

        return;
}
