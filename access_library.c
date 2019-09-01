#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"

#define READBUFSIZE 1024

static int sockfd = -1;

// Read from fd while blocking until a newline is found
// Return number of bytes read, -1 for error or 0 for EOF
static int read_until_newline(int fd, char *buffer, int max_len)
{
    int read_total = 0;
    while (read_total < max_len)
    {
        int read_n = read(fd, buffer + read_total, max_len - read_total);
        if (read_n == 0) // EOF
            return 0;
        if (read_n < 0)
        {
            if (errno == EINTR)
                read_n = 0;
            else
                return -1;
        }
        for (int i = 0; i < read_n; i++)
        {
            if (buffer[read_total + i] == '\n')
            {
                return read_total + read_n;
            }
        }
        read_total += read_n;
    }
    // newline not found after reading max_len
    return -1;
}

int os_connect(char *name)
{
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Trying to register invalid name\n");
        return false;
    }
    if (sockfd == -1)
    {
        struct sockaddr_un serv_addr;
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            perror("Client socket creation error");
            return false;
        }
        serv_addr.sun_family = AF_UNIX;
        strcpy(serv_addr.sun_path, SOCKPATH);
        // printf("Connecting to  socket...\n");
        int connected = -1;
        for (int tries = 0; tries < 4 && connected < 0; tries++)
        {
            connected = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            if (connected < 0)
            {
                // printf("Connection to socket failed retrying in 1 sec\n");
                sleep(1);
            }
        }
        if (connected < 0)
        {
            perror("Connection to socket failed too many times");
            sockfd = -1;
            return false;
        }
    }
    else
    {
        printf("Already connected, please disconnect before reconnecting\n");
        return false;
    }

    // printf("%s connected to socket\n", name);
    dprintf(sockfd, "REGISTER %s \n", name);
    // printf("Register message sent\n");

    char buffer[READBUFSIZE] = {0};
    int err = read_until_newline(sockfd, buffer, READBUFSIZE);
    if (err < 0)
    {
        fprintf(stderr, "Error reading answer of REGISTER\n");
        return false;
    }
    if (err == 0)
    {
        fprintf(stderr, "EOF reading answer of REGISTER\n");
        return false;
    }
    if (buffer[0] == 'O' && buffer[1] == 'K')
    {
        return true;
    }
    else
    {
        buffer[READBUFSIZE - 1] = '\0';
        printf("STORE recieved error: %s\n", buffer);
        return false;
    }
}

int ignore_overwrite_errors = false;
int os_store(char *name, void *block, size_t len)
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return false;
    }
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Trying to store invalid name\n");
        return false;
    }
    if (block == NULL && len > 0)
    {
        fprintf(stderr, "Store block is NULL\n");
        return false;
    }

    // printf("Sending store\n");
    dprintf(sockfd, "STORE %s %lu \n ", name, len);
    write_all(sockfd, block, len);

    char buffer[READBUFSIZE] = {0};
    int err = read_until_newline(sockfd, buffer, READBUFSIZE);
    if (err < 0)
    {
        fprintf(stderr, "Error reading answer of STORE\n");
        return false;
    }
    if (err == 0)
    {
        fprintf(stderr, "EOF reading answer of STORE\n");
        return false;
    }
    if (buffer[0] == 'O' && buffer[1] == 'K')
    {
        return true;
    }
    else
    {
        buffer[READBUFSIZE - 1] = '\0';
        char *ow_mes = "KO Another object is stored with the same name";
        if (ignore_overwrite_errors && (strncmp(buffer, ow_mes, strlen(ow_mes)) == 0))
        {
            return true;
        }
        printf("STORE recieved error: %s\n", buffer);
        return false;
    }
}

static int last_retrieve_len = -1;

void *os_retrieve(char *name)
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        last_retrieve_len = -1;
        return NULL;
    }
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Trying to retrieve invalid name\n");
        last_retrieve_len = -1;
        return NULL;
    }

    // printf("Sending retrieve\n");
    int err = dprintf(sockfd, "RETRIEVE %s \n", name);
    if (err < 0)
    {
        fprintf(stderr, "Error sending RETRIEVE\n");
        last_retrieve_len = -1;
        return NULL;
    }

    char buffer[READBUFSIZE] = {0};
    int data_read = read_until_newline(sockfd, buffer, READBUFSIZE);
    if (data_read < 0)
    {
        fprintf(stderr, "Error retrieving header for retrieve\n");
        last_retrieve_len = -1;
        return NULL;
    }
    else if (data_read == 0)
    {
        fprintf(stderr, "EOF while retrieving header for retrieve\n");
        last_retrieve_len = -1;
        return NULL;
    }
    // printf("Retrieve resulted in %s\n", buffer);
    // Buffer should contain header
    if (strncmp("DATA ", buffer, 5) != 0)
    {
        buffer[READBUFSIZE - 1] = '\0';
        if (buffer[0] == 'K' && buffer[1] == 'O')
        {
            printf("RETRIEVE recieved error: %s\n", buffer);
            printf("Retrieve %s got a KO \n", name);
        }
        else
        {
            printf("Retrieve %s got unexpected %s", name, buffer);
        }
        last_retrieve_len = -1;
        return NULL;
    }
    int len_index = strlen("DATA ");
    char lenstr[MAX_OBJ_LEN_DIGITS + 1];
    int i;
    for (i = 0; i < MAX_OBJ_LEN_DIGITS && buffer[i + len_index] != ' '; i++)
    {
        char c = buffer[i + len_index];
        if (!('0' <= c && c <= '9'))
        {
            fprintf(stderr, "Recieved invalid object len\n");
            last_retrieve_len = -1;
            return NULL;
        }
        lenstr[i] = c;
    }
    lenstr[i] = '\0';
    int store_left = atoll(lenstr);
    int data_start = i + len_index + 3;
    last_retrieve_len = store_left;
    if (store_left == 0)
    {
        last_retrieve_len = 0;
        return NULL;
    }
    // printf("Reading answer long %d\n", store_left);
    char *return_buffer = malloc(store_left);
    data_read -= data_start;
    memcpy(return_buffer, buffer + data_start, data_read);
    store_left -= data_read;
    int r = read_all(sockfd, return_buffer + data_read, store_left);
    if (r == -1)
    {
        fprintf(stderr, "Error retrieving data\n");
        free(return_buffer);
        last_retrieve_len = -1;
        return NULL;
    }
    else if (r == 0)
    {
        fprintf(stderr, "EOF retrieving data\n");
        sockfd = -1;
        free(return_buffer);
        last_retrieve_len = -1;
        return NULL;
    }
    return return_buffer;
}

int get_retrieve_len()
{
    return last_retrieve_len;
}

int os_delete(char *name)
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return false;
    }
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Deleting invalid name\n");
        return false;
    }
    dprintf(sockfd, "DELETE %s \n", name);

    char buffer[READBUFSIZE] = {0};
    int err = read_until_newline(sockfd, buffer, READBUFSIZE);
    if (err < 0)
    {
        fprintf(stderr, "Error reading answer of DELETE\n");
        return false;
    }
    if (err == 0)
    {
        fprintf(stderr, "EOF reading answer of DELETE\n");
        return false;
    }
    if (buffer[0] == 'O' && buffer[1] == 'K')
    {
        return true;
    }
    else
    {
        buffer[READBUFSIZE - 1] = '\0';
        printf("DELETE recieved error: %s\n", buffer);
        return false;
    }
}

int os_disconnect()
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return false;
    }
    dprintf(sockfd, "LEAVE \n");

    char buffer[READBUFSIZE] = {0};
    int err = read_until_newline(sockfd, buffer, READBUFSIZE);
    if (err < 0)
    {
        fprintf(stderr, "Error reading answer of LEAVE\n");
        return false;
    }
    if (err == 0)
    {
        fprintf(stderr, "EOF reading answer of LEAVE\n");
        return false;
    }
    // printf("LEAVE resulted in %s\n", buffer);
    sockfd = -1;
    if (buffer[0] == 'O' && buffer[1] == 'K')
    {
        return true;
    }
    else
    {
        buffer[READBUFSIZE - 1] = '\0';
        fprintf(stderr, "LEAVE recieved error: %s\n", buffer);
        return false;
    }
}