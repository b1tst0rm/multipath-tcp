#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

#define CONTROL_PORT = 50000
#define DATA_PORT_1 = 50001
#define DATA_PORT_2 = 50002
#define DATA_PORT_3 = 50003


int main (int argc, char const *argv[])
{
        char data[] = "0123456789"
                      "abcdefghijklmnopqrstuvwxyz"
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        int i = 0;

        for (i = 0; i < 16; i++) {
                printf("%s\n", data);
        }

        return EXIT_SUCCESS;
}
