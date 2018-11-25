/********************************
* MPTCP Server (mptcp-server.c) *
* Server file for multipath TCP *
* Author: Daniel Limanowski     *
********************************/
#include "mptcp-server.h"


/* Function: main
 * -----------------
 * Forks TCP sockets to receive payload chunks. Maps sequence numbers from
 * the client via control port to the payload chunks on the subflows. Logs
 * the mappings to a file.
 *
 * void: accepts no input
 *
 * returns: EXIT_SUCCESS upon clean exit of child processes or EXIT_FAILURE
 *     if anything went wrong.
*/
int main(void)
{
        int ctl_fd, fork1, fork2, i;
        int sf_fds[3]; /* Subflow socket file descriptor array */
        unsigned short seq_num;
        int parent_pipe[2], child_pipe[2];
        char buffer[MESSAGE_SIZE];
        FILE *log_fp; /* Log file for data/DSS mapping */
        
        /* Initialize pipes for process-to-process communication */
        if (pipe(parent_pipe) < 0) {
                perror("parent pipe");
                return EXIT_FAILURE;
        }
        if (pipe(child_pipe) < 0) {
                perror("child pipe");
                return EXIT_FAILURE;
        }
        
        printf("Server control port running on %d.\n"
               "Subflows running on ports: 1) %d\n"
               "                           2) %d\n"
               "                           3) %d\n"
               "Ready for connections.\n", CTL_PORT, SF1_PORT, SF2_PORT,
               SF3_PORT);
 
        ctl_fd = setup_socket(CTL_PORT);
        sf_fds[0] = setup_socket(SF1_PORT);
        sf_fds[1] = setup_socket(SF2_PORT);
        sf_fds[2] = setup_socket(SF3_PORT);
        
        printf("All 4 connections made successfully from client.\n");

        fork1 = fork();
        fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                /* Child processes call subflow to do their work */
                subflow(parent_pipe, child_pipe);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                return EXIT_FAILURE;
        } else {
                /* Parent process */

                /* Header for log file */
                log_fp = fopen(LOG_FILENAME, "w");
                fprintf(log_fp, "DSN,MESSAGE\n");
                
                close(parent_pipe[0]); /* Close input to write */
                close(child_pipe[1]); /* Close output to read */

                /* Write client connection descriptors to shared pipe */
                for (i = 0; i < 3; i++) { 
                        write(parent_pipe[1], &sf_fds[i], sizeof(sf_fds[i]));
                }
                
                /* Assumption was made that server did not have to run 
                   forever. TODO: Include in report and remove this comment 
                */
                while (strcmp(buffer, "STOP") != 0) {
                        /* Get sequence number from control port */
                        if (read(ctl_fd, &seq_num, sizeof(seq_num)) < 0) {
                                perror("read sequence num");
                                return EXIT_FAILURE;
                        }
                    
                        /* Get data from the childrens' pipe */
                        if (read(child_pipe[0], buffer, sizeof(buffer)) < 0) {
                                perror("read from child pipe");
                                return EXIT_FAILURE;
                        }
                        
                        if (strcmp(buffer, "STOP") != 0) {
                                /* TODO: Get shared log function in here */
                                fprintf(log_fp, "%d,%c%c%c%c\n", (int)seq_num, 
                                        buffer[0], buffer[1], buffer[2], 
                                        buffer[3]);
                                printf("Parent got SEQNUM %d MSG %s\n", 
                                       seq_num, buffer);
                        }
                }
                
                /* Wait for children to exit before exiting server app */
                while ((wait(NULL)) > 0) {}
                printf("Ending server\n");
                close(ctl_fd); /* End control connection on server */
                fclose(log_fp); /* Close log file */
        }

        return EXIT_SUCCESS;
}


/* Function: setup_socket
 * -----------------
 * Initializes a TCP subflow socket and waits for a connection to it,
 * eventually returning a connected socket (NOTE: this function will
 * block until a connection is established).
 *
 * int port: TCP port to setup port on for connections.
 *
 * returns: file descriptor of the connected socket
*/
int setup_socket(int port)
{
        int sock_fd, enable, incoming_conn;
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);
 
        /* Creating socket file descriptor */
        if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
                perror("socket");
                return EXIT_FAILURE;
        }
  
        /* Optional step that forcefully attaches socket to port */
        enable = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, 
            sizeof(enable))) { 
                perror("setsockopt");
                return EXIT_FAILURE;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; /* Represents localhost */
        address.sin_port = htons(port);

        /* Bind socket to port */
        if (bind(sock_fd, (struct sockaddr *)&address, addrlen) < 0) {
                perror("bind");
                return EXIT_FAILURE;
        }

        /* Begin listening for connections */
        if (listen(sock_fd, 3) < 0) {
                perror("listen");
                return EXIT_FAILURE;
        }

        /* Get socket for new connection */
        if ((incoming_conn = accept(sock_fd, (struct sockaddr *)
            &address, &addrlen)) < 0) {
                perror("accept");
                return EXIT_FAILURE;
        }
        
        /* Return the file descriptor of new connection */
        return incoming_conn;
}


/* Function: subflow
 * -----------------
 * Child processes (the subflows) call this function to receive data from
 * their socket and pipe it up to the parent process.
 *
 * int read_pipe[2]: The child reads from this pipe, for example to get 
 *     its connection socket. The parent writes to this pipe.
 * int write_pipe[2]: The child writes the connection data to this pipe.
 *
 * returns: void (nothing)
*/
void subflow(int read_pipe[2], int write_pipe[2])
{
        int sf_fd, incoming_conn;
        char buffer[MESSAGE_SIZE];
        
        close(read_pipe[1]);
        close(write_pipe[0]);
        
        /* Get the subflow connection socket file descriptor */
        read(read_pipe[0], &sf_fd, sizeof(sf_fd));

        while (strcmp(buffer, "STOP") != 0) {
                /* Child reads from the TCP socket and writes to the pipe */
                read(sf_fd, buffer, sizeof(buffer));
                write(write_pipe[1], buffer, sizeof(buffer));
        }

        /* Perform cleanup by closing file descriptors */
        close(sf_fd);
        close(read_pipe[0]);
        close(write_pipe[1]);
}
