#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/sysinfo.h>
#define PORT 44444

// Returns uptime of server in seconds
void fetch_uptime(char **uptime)
{
        *uptime = malloc(64); // uptime typically only returns ~60 characters
        char buffer[64];

        FILE *fp;
        fp = popen("/usr/bin/uptime", "r");
    
        if (fp == NULL) {
                strcpy(*uptime, "Unable to retrieve uptime");
        }

        fgets(buffer, sizeof(buffer), fp);
        pclose(fp);
        strcpy(*uptime, buffer);
}

// Simple TCP server that returns its uptime to clients
int main(int argc, char const *argv[])
{
        int server_fd, new_socket, valread;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        char *message;
        int opt = 1;
        char buffer[64] = {0};
 
        // Creating socket file descriptor 
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
                printf("Failed to create socket.\n");
                return -1;
        }
  
        // Optional step that forcefully attaches socket to PORT
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { 
                printf("Failed at setsockopt.\n");
                return -1;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; // represents localhost
        address.sin_port = htons(PORT);

        // Bind socket to PORT
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                printf("Fail to bind socket.\n");
                return -1;
        }

        if (listen(server_fd, 3) < 0) {
                printf("Failed at listen.\n");
                return -1;
        }

        printf("Server running on %d. Ready for connections.\n", PORT);

        // Run the server forever
        while(1) {
                // Gets the next connection
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                        printf("Failed at accept.\n");
                        return -1;
                }
                valread = read(new_socket, buffer, sizeof(buffer));
                printf("Request received (%s). Responding with uptime.\n", buffer);
                fetch_uptime(&message);
                send(new_socket, message, strlen(message), 0);
                printf("Message sent.\n");
        }
}
