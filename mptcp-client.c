/********************************
* MPTCP Client (mptcpclient.c)  *
* Client file for multipath TCP *
* Author: Daniel Limanowski     *
********************************/


#include "mptcp-client.h"


int main()
{
        int ctl_fd, ctl_retval, i;
        char buffer[1024] = {0};
        int init_pipe[2]; // Parent writes, child reads...only used to administer child IDs
        int child_pipes[3][2]; // Child writes, parent reads 
        int parent_pipes[3][2]; // Parent writes, child reads
        FILE *log_fp = fopen(LOG_FILENAME, "w"); /* Log file for data/DSS mapping */

        // Create pipes for process-to-process communication
        for (i = 0; i < 3; i++) {
                if (pipe(child_pipes[i]) < 0 || pipe(parent_pipes[i]) < 0) {
                        perror("private pipe");
                        exit(EXIT_FAILURE);
                }
        }
        if (pipe(init_pipe) < 0) {
                perror("parent pipe");
                exit(EXIT_FAILURE);
        }

        /* Set up the connections (4 in total) to the server application */
        ctl_fd = create_subflow(CTL_PORT);
        int sf_fd1 = create_subflow(DATA_PORT_1);
        int sf_fd2 = create_subflow(DATA_PORT_2);
        int sf_fd3 = create_subflow(DATA_PORT_3);

        /* Fork off the child processes to handle subflows */
        int fork1 = fork();
        int fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                // child process
                subflow(init_pipe, child_pipes, parent_pipes);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else {
                /* Parent process */
                /* Header for log file */
                fprintf(log_fp, "DSS,MESSAGE\n");

                char resp1, resp2, resp3;
                close(init_pipe[0]); // close input to write
                for (i = 0; i < 3; i++) {
                        close(child_pipes[i][1]); // close write end
                        close(parent_pipes[i][0]); // close read end
                }
                
                // Write child IDs to shared pipe
                unsigned short ids[3] = {0, 1, 2};
                for (i = 0; i < 3; i++) {
                        if (write(init_pipe[1], &ids[i], sizeof(unsigned short)) < 0) {
                                perror("can't write to init");
                                exit(EXIT_FAILURE);
                        }
                }
                close(init_pipe[1]); // close pipe as no longer used

                // Write connection FDs to private pipes
                if (write(parent_pipes[0][1], &sf_fd1, sizeof(int)) < 0) {
                        perror("can't write sf to child 0");
                        exit(EXIT_FAILURE);
                }
                if (write(parent_pipes[1][1], &sf_fd2, sizeof(int)) < 0) {
                        perror("can't write sf to child 1");
                        exit(EXIT_FAILURE);
                }
                if (write(parent_pipes[2][1], &sf_fd3, sizeof(int)) < 0) {
                        perror("can't write sf to child 2");
                        exit(EXIT_FAILURE);
                }

                // Get child process ACKs
                read(child_pipes[0][0], &resp1, sizeof(char));
                read(child_pipes[1][0], &resp2, sizeof(char));
                read(child_pipes[2][0], &resp3, sizeof(char));
                printf("Responses: %c %c %c\n", resp1, resp2, resp3);

                // Write message to the children round-robin style
                int child_index = 0;
                unsigned short num = 0; // represents the DSS (overarching sequence num of transmission
                char msg[ROUND_ROBIN_SIZE];
                for (i = 0; i < 992; i = i + 4) {
                        //strncpy(msg, &total_msg[i], ROUND_ROBIN_SIZE);
                        msg[0] = payload[i];
                        msg[1] = payload[i+1];
                        msg[2] = payload[i+2];
                        msg[3] = payload[i+3];
 
                        printf("Parent sending %d to ctl and %c%c%c%c to child\n", num, msg[0], msg[1], msg[2],msg[3]);
                        // Parent writes the sequence number followed by the ROUND_ROBIN_SIZE message
                        // - the child ACKs by returning the sequence number...if sequence number
                        // is wrong, then the parent throws an error and exits
                        write(parent_pipes[child_index][1], &num, sizeof(unsigned short));
                        write(parent_pipes[child_index][1], &msg, ROUND_ROBIN_SIZE);

                        fprintf(log_fp, "%d,%c%c%c%c\n", (int)num, msg[0], msg[1], msg[2], msg[3]);  

                        // send sequence number to control port on server
                        if (send(ctl_fd, &num, sizeof(num), 0) < 0) {
                                perror("sending over control port");
                                break;
                        }

                        // get confirmation back from the child process
                        unsigned short response = 0;
                        if (read(child_pipes[child_index][0], &response, sizeof(response)) < 0) {
                                perror("getting child ack");
                                break;
                        }

                        if (response != num) {
                                perror("sending message");
                                break;
                        }

                        if (child_index == 2) {
                                child_index = 0;
                        } else {
                                child_index++;
                        }
                        num++; // increment sequence number before sending next packet
                }

                // End the children with "STOP"
                printf("Parent sending STOP to children\n");
                char stop[ROUND_ROBIN_SIZE] = "STOP";
                write(parent_pipes[0][1], &num, sizeof(unsigned short));
                write(parent_pipes[0][1], &stop, ROUND_ROBIN_SIZE);
                write(parent_pipes[1][1], &num, sizeof(unsigned short));
                write(parent_pipes[1][1], &stop, ROUND_ROBIN_SIZE);
                write(parent_pipes[2][1], &num, sizeof(unsigned short));
                write(parent_pipes[2][1], &stop, ROUND_ROBIN_SIZE);

                // Wait until all child processes finish executing
                int wpid, status=0;
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
void subflow(int init_pipe[2], int write_pipes[3][2], int read_pipes[3][2])
{
        unsigned short child_id; /* child gets an ID that tells them what index in 
                                 array to use to read from "their" pipe */
        int serv_fd = 0, sent;
        char msg_to_send[ROUND_ROBIN_SIZE];
        char yes = 'Y';
        unsigned short seq_num;

        // First the subflow will read in its ID
        close(init_pipe[1]); // close the write end of the read pipe
        if (read(init_pipe[0], &child_id, sizeof(unsigned short)) < 0) {
                perror("can't get child_id");
                exit(EXIT_FAILURE);
        }
        close(init_pipe[0]); // close out the init pipe as subflow will no longer use it
       
        // Next, subflow accesses its private pipe and gets its socket descriptor
        close(read_pipes[child_id][1]); // close the write end of the read pipe
        if (read(read_pipes[child_id][0], &serv_fd, sizeof(int)) < 0) {
                perror("can't get serv_fd");
                exit(EXIT_FAILURE);
        }
        
        // acknowledge receipt of socket fd by writing y to pipe (y = yes)
        close(write_pipes[child_id][0]);
        if (write(write_pipes[child_id][1], &yes, sizeof(char)) < 0) {
                perror("cannot write to parent");
                exit(EXIT_FAILURE);
        }

        // Parent process will send child processes ROUND_ROBIN_SIZE byte chunks to send to
        // the server. Once the entirety of the message has been sent, the
        // parent will send "STOP" to indicate that the child should die 
        while (strcmp(msg_to_send, "STOP") != 0) {
                read(read_pipes[child_id][0], &seq_num, sizeof(seq_num)); // fetch sequence num
                read(read_pipes[child_id][0], &msg_to_send, ROUND_ROBIN_SIZE); // fetch message
                printf("Child %d is sending message: %s\n", (int)child_id, msg_to_send);
                sent = send(serv_fd, msg_to_send, sizeof(msg_to_send), 0); // send message to server
                if (sent != -1) {
                        write(write_pipes[child_id][1], &seq_num, sizeof(seq_num)); // return the sequence number to parent
                } else {
                        perror("send to server");
                        break; 
                }
        }
        return;
}
