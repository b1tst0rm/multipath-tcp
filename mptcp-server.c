#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <wait.h>

#define CTL_PORT 50000
#define SF1_PORT 50001
#define SF2_PORT 50002
#define SF3_PORT 50003


// Function prototypes
int setup_socket(int port);
void subflow(int read_pipe[2], int write_pipe[2]);


int main(int argc, char const *argv[])
{
        int ctl_fd, new_socket, valread, enable, 
            sf1_fd, sf2_fd, sf3_fd, fork1, fork2;
        char buffer[64] = {0};
        int parent_pipe[2], child_pipe[2];
        
        // Create pipes for process-to-process communication
        if (pipe(parent_pipe) < 0) {
                perror("parent pipe");
                exit(EXIT_FAILURE);
        }

        if (pipe(child_pipe) < 0) {
                perror("child pipe");
                exit(EXIT_FAILURE);
        }
        
        printf("Server control port running on %d.\n"
               "Subflows running on ports: 1) %d\n"
               "                           2) %d\n"
               "                           3) %d\n"
               "Ready for connections.\n", CTL_PORT, SF1_PORT, SF2_PORT,
               SF3_PORT);
 
        ctl_fd = setup_socket(CTL_PORT);
        sf1_fd = setup_socket(SF1_PORT);
        sf2_fd = setup_socket(SF2_PORT);
        sf3_fd = setup_socket(SF3_PORT);
        
        printf("All 4 connections made successfully from client.\n");

        // Fork subflow listeners
        fork1 = fork();
        fork2 = fork();
        if (fork1 == 0 || fork2 == 0) {
                // Child processes here
                subflow(parent_pipe, child_pipe);
        } else if (fork1 < 0 || fork2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else {
                char resp[3];
                close(parent_pipe[0]); // close input to write
                close(child_pipe[1]); // close output to read
                write(parent_pipe[1], &sf1_fd, sizeof(int));
                write(parent_pipe[1], &sf2_fd, sizeof(int));
                write(parent_pipe[1], &sf3_fd, sizeof(int));
                read(child_pipe[0], &resp[0], sizeof(char));
                read(child_pipe[0], &resp[1], sizeof(char));
                read(child_pipe[0], &resp[2], sizeof(char));

                if (resp[0] == 'Y' && resp[1] == 'Y' && resp[2] == 'Y') {
                        printf("Subflow connections are connected\n");
                }
                
                // TODO: assumption was made that server did not have to run forever
                // Gets data connection
                valread = read(ctl_fd, buffer, sizeof(buffer));
                printf("Client control connection established (%s)\n", buffer);
                int wpid, status=0;
                while ((wpid = wait(&status)) > 0) {}
                printf("Ending server\n");
                close(ctl_fd);
        }
}

// Note: this function blocks until a connection is established to it
int setup_socket(int port)
{
        int sock_fd, enable, incoming_conn;
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);
 
        // Creating socket file descriptor 
        if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
                perror("socket");
                return EXIT_FAILURE;
        }
  
        // Optional step that forcefully attaches socket to port
        enable = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) { 
                perror("setsockopt");
                return EXIT_FAILURE;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; // represents localhost
        address.sin_port = htons(port);

        // Bind socket to port
        if (bind(sock_fd, (struct sockaddr *)&address, addrlen) < 0) {
                perror("bind");
                return EXIT_FAILURE;
        }

        // Begin listening for connections
        if (listen(sock_fd, 3) < 0) {
                perror("listen");
                return EXIT_FAILURE;
        }

        if ((incoming_conn = accept(sock_fd, (struct sockaddr *)
            &address, &addrlen)) < 0) {
                perror("accept");
                return EXIT_FAILURE;
        }
        
        // Return the file descriptor of CONNECTED file descriptor
        return incoming_conn;
}


void subflow(int read_pipe[2], int write_pipe[2]) {
        int sf_fd = 0, incoming_conn;
        close(read_pipe[1]);
        close(write_pipe[0]);

        char buffer[4];

        // Get the subflow connection socket file descriptor
        read(read_pipe[0], &sf_fd, sizeof(int));

        // Acknowledge receipt of socket fd by writing Y to pipe (Y = yes)
        char yes = 'Y';
        write(write_pipe[1], &yes, sizeof(char));
        
        int valread = read(sf_fd, buffer, sizeof(buffer));

        printf("child got %s\n", buffer);

        close(sf_fd);
        return;
}
