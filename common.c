/********************************
* Common code (common.c)        *
* Shared code that the          *
* server and client both use.   *
* Author: Daniel Limanowski     *
********************************/
#include "common.h"


/* Function: add_to_log
 * -----------------
 * Adds a comma-separated line to the logfile given a Data-Sequence-Number 
 * (DSN) and a message.
 *
 * FILE *log_fp: Already-opened pointer to log-file
 * unsigned short dsn: data sequence number for line in log
 * char msg[ROUND_ROBIN_SIZE]: Message chunk associated with DSN for log line
 *
 * returns: 1 if line added successfully, -1 otherwise
*/
int add_to_log(FILE *log_fp, unsigned short dsn, char msg[ROUND_ROBIN_SIZE])
{
        if (fprintf(log_fp, "%d,%c%c%c%c\n", (int)dsn, msg[0], msg[1], msg[2],
                msg[3]) < 0) {
                return 1;
        } else {
                return -1;
        }
}
