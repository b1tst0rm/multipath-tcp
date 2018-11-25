/********************************
* MPTCP Client (mptcpclient.c)  *
* Client file for multipath TCP *
* Author: Daniel Limanowski     *
********************************/
#include "mptcp-client.h"


/* Function: main
 * -----------------
 * Forks TCP subflows and sends payload data to each subflow while sending
 * mapping information to the server over control port and logging the info.
 *
 * void: accepts no input
 *
 * returns: EXIT_SUCCESS upon clean exit of child processes or EXIT_FAILURE
 *     if anything went wrong.
*/
int main(void)
{
        int ctl_fd, fork1, fork2, i, j, child_index;
        int sf_fds[3]; /* Subflow socket file descriptor array */
        int init_pipe[2]; /* Only used to administer child IDs */
        int child_pipes[3][2], parent_pipes[3][2];
        FILE *log_fp; /* Log file for data/DSS mapping */
        char child_ack;
        char msg[ROUND_ROBIN_SIZE];
        const unsigned short ids[3] = {0, 1, 2};
        unsigned short dsn, response;

        /* Initialize pipes for process-to-process communication */
        for (i = 0; i < 3; i++) {
                if (pipe(child_pipes[i]) < 0 || pipe(parent_pipes[i]) < 0) {
                        perror("pipe arrays initialization");
                        return EXIT_FAILURE;
                }
        }
        if (pipe(init_pipe) < 0) {
                perror("init pipe initialization");
                return EXIT_FAILURE;
        }

        /* Set up the connections (4 in total) to the server application */
        ctl_fd = create_subflow(CTL_PORT);
        sf_fds[0] = create_subflow(DATA_PORT_1);
        sf_fds[1] = create_subflow(DATA_PORT_2);
        sf_fds[2] = create_subflow(DATA_PORT_3);

        /* Fork off the child processes to handle subflows */
        fork1 = fork();
        fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                /* Child process calls subflow to do its work */
                subflow(init_pipe, child_pipes, parent_pipes);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                return EXIT_FAILURE;
        } else {
                /* Parent process */

                /* Open and initialize log file */
                log_fp = fopen(LOG_FILENAME, "w");
                fprintf(log_fp, "DSS,MESSAGE\n");

                /* Close proper ends of pipes to communicate with children */
                close(init_pipe[0]);
                for (i = 0; i < 3; i++) {
                        close(child_pipes[i][1]);
                        close(parent_pipes[i][0]);
                }
                
                /* Write child IDs to shared pipe */
                for (i = 0; i < 3; i++) {
                        if (write(init_pipe[1], &ids[i], sizeof(ids[i])) < 0) {
                                perror("write to init pipe");
                                return EXIT_FAILURE;
                        }
                }
                close(init_pipe[1]); /* Close pipe as no longer used */

                /* Write server socket file descriptors to private pipes */
                for (i = 0; i < 3; i++) {
                        if (write(parent_pipes[i][1], &sf_fds[i], 
                            sizeof(sf_fds[i])) < 0) {
                                perror("can't write sf_fd to child");
                                return EXIT_FAILURE;
                        }
                }

                /* Get child process ACKs before continuing */
                j = 1;
                child_ack = '\0';
                for (i = 0; i < 3; i++) {
                        read(child_pipes[i][0], &child_ack, sizeof(child_ack));
                        if (child_ack != 'Y') {
                                j = -1;
                        }
                        child_ack = '\0'; /* Reset value */
                }

                if (j != 1) {
                        /* A child did not acknowledge properly, quit app */
                        perror("Child did not ack");
                        return EXIT_FAILURE;
                }

                /* Write payload to the children round-robin style */
                child_index = 0;
                dsn = 0;
                for (i = 0; i < 992; i = i + 4) {
                        strncpy(msg, &payload[i], ROUND_ROBIN_SIZE);
                        
                        /* Parent writes the sequence number followed by 
                           ROUND_ROBIN_SIZE message. The child ACKs by 
                           returning the sequence number...if sequence number
                           is wrong, then the parent throws an error and exits
                        */
                        printf("Parent sending %d to ctl and %c%c%c%c to "
                               "child\n", dsn, msg[0], msg[1], msg[2],msg[3]);
                        write(parent_pipes[child_index][1], &dsn, sizeof(dsn));
                        write(parent_pipes[child_index][1], &msg, sizeof(msg));

                        /* Log the mapping of DSN to bytes */
                        fprintf(log_fp, "%d,%c%c%c%c\n", (int)dsn, 
                                msg[0], msg[1], msg[2], msg[3]);  

                        /* Send sequence number to control port on server */
                        if (send(ctl_fd, &dsn, sizeof(dsn), 0) < 0) {
                                perror("sending over control port");
                                break;
                        }

                        /* Get confirmation back from the child process */
                        response = 0;
                        if (read(child_pipes[child_index][0], &response, 
                            sizeof(response)) < 0) {
                                perror("getting child ack");
                                break;
                        }

                        if (response != dsn) {
                                perror("sending message");
                                break;
                        }

                        if (child_index == 2) {
                                child_index = 0;
                        } else {
                                child_index++;
                        }

                        /* increment sequence num before sending next packet */
                        dsn++;
                }

                /* End the children's transmission with "STOP" */
                printf("Parent sending STOP to children\n");
                for (i = 0; i < 3; i++) {
                        write(parent_pipes[i][1], &dsn, sizeof(dsn));
                        write(parent_pipes[i][1], "STOP", 4);
                }

                /* Wait until all child processes finish executing */
                while ((wait(NULL)) > 0) {}
                printf("Ending client\n"); 
                close(ctl_fd); /* End control connection to server */
                fclose(log_fp); /* Close log file */
        }

        return EXIT_SUCCESS;
}


/* Function: create_subflow
 * -----------------
 * Initializes a TCP subflow connection to the server by setting up a socket
 * and connecting to SERV_IP.
 *
 * int port: TCP port to connect to on the server host
 *
 * returns: file descriptor of the connected socket
*/
int create_subflow(int port)
{
        int sf_fd, enable;
        struct sockaddr_in serv_addr;

        /* Create the TCP/IP socket */
        if ((sf_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }

        /* Configure socket to be reusable */
        enable = 1;
        if (setsockopt(sf_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
            sizeof(enable)) < 0) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }

        /* Set up server information */
        /* Allocate memory to store address */
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_port = htons(port); /* Network byte order conversion */
        serv_addr.sin_family = AF_INET; /* IPv4 */

        /* Convert IP address from text to binary */
        if (inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr) != 1) {
                perror("inet_pton");
                exit(EXIT_FAILURE);
        }

        /* Must have this sleep in here as a delay before continuing otherwise 
           the socket cannot connect in time.
        */
        sleep(0.01);

        /* Connect to the server application */
        if (connect(sf_fd, (struct sockaddr *)&serv_addr, 
            sizeof(serv_addr)) < 0) {
                perror("connect");
                exit(EXIT_FAILURE);
        }

        return sf_fd;
}


/* Function: subflow
 * -----------------
 * Child processes (the subflows) call this function to receive data from
 * parent process and send over to server.
 *
 * int init_pipe[2]: Initialization pipe used to get a child's "ID" so it
 *     knows which of the read/write pipes to use
 * int write_pipes[3][2]: Array (one for each subflow) of pipes for child 
 *     to write to (writes back the received DSS)
 * int read_pipes[3][2]: Array of pipes for child to read from (data)
 *
 * returns: void (nothing)
*/
void subflow(int init_pipe[2], int write_pipes[3][2], int read_pipes[3][2])
{
        unsigned short id; /* ID that inidicates pipes to use in arrays */
        int serv_fd = 0, sent;
        char msg_to_send[ROUND_ROBIN_SIZE];
        char yes = 'Y';
        unsigned short seq_num;

        /* Subflow reads in its ID */
        close(init_pipe[1]); /* Close write end of read pipe */
        if (read(init_pipe[0], &id, sizeof(id)) < 0) {
                perror("can't get child_id");
                exit(EXIT_FAILURE);
        }
        close(init_pipe[0]); /* Close out init pipe as no longer needed */

        /* Subflow accesses its private pipe and gets its socket descriptor */
        close(read_pipes[id][1]); /* Close the write end of the read pipe */
        if (read(read_pipes[id][0], &serv_fd, sizeof(serv_fd)) < 0) {
                perror("can't get serv_fd");
                exit(EXIT_FAILURE);
        }
 
        /* Acknowledge receipt of socket fd by writing 'Y' to pipe */
        close(write_pipes[id][0]);
        if (write(write_pipes[id][1], &yes, sizeof(yes)) < 0) {
                perror("cannot write to parent");
                exit(EXIT_FAILURE);
        }

        /* Parent process will send DSNs followed by ROUND_ROBIN_SIZE-byte 
           chunks to send to the server. Once the entirety of the message has 
           been sent, the parent will send "STOP" to indicate that the child
           should die.
        */
        while (strcmp(msg_to_send, "STOP") != 0) {
                /* Fetch sequence number from pipe */
                read(read_pipes[id][0], &seq_num, sizeof(seq_num));
                
                /* Fetch message from pipe */
                read(read_pipes[id][0], &msg_to_send, ROUND_ROBIN_SIZE);
                
                /* Send message to server */
                sent = send(serv_fd, msg_to_send, sizeof(msg_to_send), 0);
                
                if (sent != -1) {
                        /* If the message is sent to server, return DSN to
                           parent as an acknowledgement */ 
                        write(write_pipes[id][1], &seq_num, sizeof(seq_num));
                } else {
                        perror("send to server");
                        break;
                }
        }
}
