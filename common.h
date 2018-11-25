/********************************
* Shared header (common.h)      *
* Header file for common.c.     *
* Header contains prototypes,   *
* includes, & defines that both *
* server and client use.        *
* Author: Daniel Limanowski     *
********************************/
/* INCLUDES */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <netinet/in.h>


/* DEFINES */
#define CTL_PORT 50000    // Control Port for DSN
#define DATA_PORT_1 50001 // Data subflows 1-3
#define DATA_PORT_2 50002
#define DATA_PORT_3 50003
#define ROUND_ROBIN_SIZE 4


/* FUNCTION PROTOTYPES */
int add_to_log(FILE *log_fp, unsigned short dsn, char msg[ROUND_ROBIN_SIZE]);
