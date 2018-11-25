/********************************
* MPTCP Server (mptcp-server.h) *
* Header file for server app    *
* Author: Daniel Limanowski     *
********************************/
/* INCLUDES */
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <wait.h>


/* DEFINES */
#define CTL_PORT 50000
#define SF1_PORT 50001
#define SF2_PORT 50002
#define SF3_PORT 50003
#define MESSAGE_SIZE 4 /* The client's "round robin size" */
#define LOG_FILENAME "log_server.txt"


/* FUNCTION PROTOTYPES */
int setup_socket(int port);
void subflow(int read_pipe[2], int write_pipe[2]);
