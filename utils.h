#include <errno.h>
#include <stdlib.h>

#define true 1
#define false 0

#define MAX_FILENAME_LEN 255  // POSIX name limtis
#define MAX_OBJ_LEN_DIGITS 10 // 1 Gb
#define MAX_HEADER_LEN 300    // Filename + command + obj len
#define SOCKPATH "objstore.sock"
#define STORAGEPATH "data/"
#define LOCKFILENAME ".lock" // Used to mark actively open folders

// perror with given error number
#define print_error_n(en, msg) \
    do                         \
    {                          \
        errno = en;            \
        perror(msg);           \
    } while (0);

// Write all len bytes to fd, blocks until done
// Returns 0 for EOF, 1 for success and -1 for error
int write_all(int fd, char *buf, int len);

// Write all len bytes to fd, blocks until done
// Returns 0 for EOF, 1 for success and -1 for error
int read_all(int fd, char *buf, int len);
