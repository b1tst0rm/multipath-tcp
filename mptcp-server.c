#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>


#define CTL_PORT 50000


int main(int argc, char const *argv[])
{
        int ctl_fd, new_socket, valread, enable;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        char *message = "hi";
        char buffer[64] = {0};
 
        // Creating socket file descriptor 
        if ((ctl_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
                perror("socket");
        }
  
        // Optional step that forcefully attaches socket to PORT
        enable = 1;
        if (setsockopt(ctl_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) { 
                perror("setsockopt");
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; // represents localhost
        address.sin_port = htons(CTL_PORT);

        // Bind socket to CTL_PORT
        if (bind(ctl_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("bind");
        }

        if (listen(ctl_fd, 3) < 0) {
                perror("listen");
        }

        printf("Server running on %d. Ready for connections.\n", CTL_PORT);

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
