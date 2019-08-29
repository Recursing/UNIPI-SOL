#include <unistd.h>
#include <errno.h>
#include <stdio.h>

int write_all(int fd, char *buffer, int len)
{
    int written_total = 0;
    while (written_total < len)
    {
        int written = write(fd, buffer + written_total, len - written_total);
        if (written == 0) // EOF
            return 0;
        if (written < 0)
        {
            if (errno == EINTR)
                written = 0;
            else
                return -1;
        }
        written_total += written;
    }
    return 1;
}

int read_all(int fd, char *buffer, int len)
{
    int read_total = 0;
    while (read_total < len)
    {
        int read_n = read(fd, buffer + read_total, len - read_total);
        if (read_n == 0) // EOF
            return 0;
        if (read_n < 0)
        {
            if (errno == EINTR)
                read_n = 0;
            else
                return -1;
        }
        read_total += read_n;
    }
    return 1;
}