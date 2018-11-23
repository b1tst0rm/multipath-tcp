#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>


#define CTL_PORT 50000
#define SF1_PORT 50001
#define SF2_PORT 50002
#define SF3_PORT 50003


// Function prototypes
int setup_socket(int port);


int main(int argc, char const *argv[])
{
        int ctl_fd, new_socket, valread, enable, sf1_fd, sf2_fd, sf3_fd;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        char *message = "hi";
        char buffer[64] = {0};
 
        ctl_fd = setup_socket(CTL_PORT);
        sf1_fd = setup_socket(SF1_PORT);
        sf2_fd = setup_socket(SF2_PORT);
        sf3_fd = setup_socket(SF3_PORT);

        printf("Server control port running on %d.\n"
               "Subflows running on ports: 1) %d\n"
               "                           2) %d\n"
               "                           3) %d\n"
               "Ready for connections.\n", CTL_PORT, SF1_PORT, SF2_PORT, SF3_PORT);

        // Run the server forever
        while(1) {
                // Gets the next connection
                if ((new_socket = accept(ctl_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                        perror("accept");
                }
                valread = read(new_socket, buffer, sizeof(buffer));
                printf("Request received (%s)\n", buffer);
                send(new_socket, message, strlen(message), 0);
                printf("Response sent.\n");
        }
}

int setup_socket(int port)
{
        int sock_fd, enable;
        struct sockaddr_in address;
 
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
        if (bind(sock_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("bind");
                return EXIT_FAILURE;
        }

        // Begin listening for connections
        if (listen(sock_fd, 3) < 0) {
                perror("listen");
                return EXIT_FAILURE;
        }

        // Return the file descriptor of the newly-made socket
        return sock_fd;
}
