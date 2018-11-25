/********************************
* MPTCP Server (mptcp-server.h) *
* Header file for server app    *
* Author: Daniel Limanowski     *
********************************/
/* INCLUDES */
#include "common.h"


/* DEFINES */
#define LOG_FILENAME "log_server.txt"


/* FUNCTION PROTOTYPES */
int setup_socket(int port);
void subflow(int read_pipe[2], int write_pipe[2]);
