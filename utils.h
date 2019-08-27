#include <errno.h>
#include <stdlib.h>
#define true 1
#define false 0

#define MAX_FILENAME_LEN 255
#define MAX_OBJ_LEN_DIGITS 11 // 10 Gb
#define MAX_HEADER_LEN 300    // Filename + command + obj len
#define SOCKPATH "objstore.sock"
#define STORAGEPATH "store/"

// TODO more graceful exit
#define handle_error_n(en, msg) \
    do                          \
    {                           \
        errno = en;             \
        perror(msg);            \
        exit(EXIT_FAILURE);     \
    } while (0);

int write_all(int fd, char *buf, int len);

int read_all(int fd, char *buf, int len);
