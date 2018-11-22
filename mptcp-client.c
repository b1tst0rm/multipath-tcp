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
        pid_t subflow_procs[3];

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

        for (i = 0; i < 3; i++) {
                if ((subflow_procs[i] = fork()) < 0) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                } else if (subflow_procs[i] == 0) {
                        // Child in here
                }
        }

        return EXIT_SUCCESS;
}
