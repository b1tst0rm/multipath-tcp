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
#define ROUND_ROBIN_SIZE 4

// FUNCTION PROTOTYPES
int create_subflow(int port);
void subflow(int init_pipe[2], int write_pipe[2], int read_pipes[3][2]);


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
        int private_pipes[3][2]; // Child writes, parent reads

        // Create pipes for process-to-process communication
        for (i = 0; i < 3; i++) {
                if (pipe(private_pipes[i]) < 0) {
                        perror("private pipe");
                        exit(EXIT_FAILURE);
                }
        }

        if (pipe(parent_pipe) < 0) {
                perror("parent pipe");
                exit(EXIT_FAILURE);
        }

        if (pipe(child_pipe) < 0) {
                perror("child pipe");
                exit(EXIT_FAILURE);
        }
        
        ctl_fd = create_subflow(CTL_PORT);
        int sf_fd1 = create_subflow(DATA_PORT_1);
        int sf_fd2 = create_subflow(DATA_PORT_2);
        int sf_fd3 = create_subflow(DATA_PORT_3);

        int fork1 = fork();
        int fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                // child process
                subflow(parent_pipe, child_pipe, private_pipes);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else {
                // parent process
                char resp1, resp2, resp3;
                char msg1[ROUND_ROBIN_SIZE] = "sto1";
                char msg2[ROUND_ROBIN_SIZE] = "sto2";
                char msg3[ROUND_ROBIN_SIZE] = "sto3";
                close(parent_pipe[0]); // close input to write
                close(private_pipes[0][0]); // close input to write
                close(private_pipes[1][0]); // close input to write
                close(private_pipes[2][0]); // close input to write
                close(child_pipe[1]); // close output to read

                unsigned short ids[3];
                ids[0] = 0;
                ids[1] = 1;
                ids[2] = 2;
                // Write child IDs to shared pipe
                write(parent_pipe[1], &ids[0], sizeof(unsigned short));
                write(parent_pipe[1], &ids[1], sizeof(unsigned short));
                write(parent_pipe[1], &ids[2], sizeof(unsigned short));

                close(parent_pipe[1]); // close pipe as no longer used

                // Write connection FDs to private pipes
                write(private_pipes[0][1], &sf_fd1, sizeof(int));
                write(private_pipes[1][1], &sf_fd2, sizeof(int));
                write(private_pipes[2][1], &sf_fd3, sizeof(int));

                // Get child process ACKs
                read(child_pipe[0], &resp1, sizeof(char));
                read(child_pipe[0], &resp2, sizeof(char));
                read(child_pipe[0], &resp3, sizeof(char));
                printf("Responses: %c %c %c\n", resp1, resp2, resp3);

                // Write message to the children
                write(private_pipes[0][1], &msg1, ROUND_ROBIN_SIZE);
                write(private_pipes[1][1], &msg2, ROUND_ROBIN_SIZE);
                write(private_pipes[2][1], &msg3, ROUND_ROBIN_SIZE);

                // End the children with "STOP"
                char stop[ROUND_ROBIN_SIZE] = "STOP";
                write(private_pipes[0][1], &stop, ROUND_ROBIN_SIZE);
                write(private_pipes[1][1], &stop, ROUND_ROBIN_SIZE);
                write(private_pipes[2][1], &stop, ROUND_ROBIN_SIZE);

 
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

        sleep(0.01); // TODO: must have this sleep (or a printf) in here as a delay before continuing otherwise
        // the socket cannot connect in time. trying googling this...

        // Connect to the server application
        if (connect(sf_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("connect");
                exit(EXIT_FAILURE);
        }

        // Return the file descriptor of connected socket 
        return sf_fd;
}


// Child processes call this to execute MP subflow code
/* TODO: Cleanup...thought: have a shared read_pipe and an array of child-process-specific
read_pipes. The children get their "ID" from the shared pipe and then access 
their index in the array of read_pipes */
void subflow(int init_pipe[2], int write_pipe[2], int read_pipes[3][2])
{

        unsigned short child_id; /* child gets an ID that tells them what index in 
                                 array to use to read from "their" pipe */
        int serv_fd = 0;
        char msg_to_send[4];

        // First the subflow will read in its ID
        close(init_pipe[1]); // close the write end of the read pipe
        read(init_pipe[0], &child_id, sizeof(unsigned short));
        close(init_pipe[0]); // close out the init pipe as subflow will no longer use it
       
        // Next, subflow accesses its private pipe and gets its socket descriptor
        close(read_pipes[child_id][1]); // close the write end of the read pipe
        read(read_pipes[child_id][0], &serv_fd, sizeof(int));
        
        // Acknowledge receipt of socket fd by writing Y to pipe (Y = yes)
        char yes = 'Y';
        close(write_pipe[0]);
        write(write_pipe[1], &yes, sizeof(char));

        // Parent process will send child processes ROUND_ROBIN_SIZE byte chunks to send to
        // the server. Once the entirety of the message has been sent, the
        // parent will send "STOP" to indicate that the child should die 
        while (strcmp(msg_to_send, "STOP") != 0) {
                read(read_pipes[child_id][0], &msg_to_send, ROUND_ROBIN_SIZE);
                send(serv_fd, msg_to_send, strlen(msg_to_send), 0);
        }

        return;
}
