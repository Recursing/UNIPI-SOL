#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"

static int sockfd = -1;

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
        printf("Connecting to  socket...\n");
        int err = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (err < 0)
        {
            perror("Connection to socket failed");
            sockfd = -1;
            return false;
        }
    }

    printf("%s connected to socket\n", name);
    dprintf(sockfd, "REGISTER %s \n", name);
    printf("Register message sent\n");

    char buffer[1024] = {0};
    read(sockfd, buffer, 1024);
    buffer[1023] = '\0';
    printf("Register resulted in %s", buffer);
    return (buffer[0] == 'O' && buffer[1] == 'K');
}

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

    printf("Sending store\n");
    dprintf(sockfd, "STORE %s %lu \n ", name, len);
    write_all(sockfd, block, len);

    // TODO read answer
    char buffer[1024] = {0};
    read(sockfd, buffer, 1024);
    printf("Store resulted in %s\n", buffer);
    return (buffer[0] == 'O' && buffer[1] == 'K');
}

void *os_retrieve(char *name)
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return false;
    }
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Trying to retrieve invalid name\n");
        return false;
    }

    printf("Sending retrieve\n");
    int err = dprintf(sockfd, "RETRIEVE %s \n", name);
    if (err < 0)
    {
        fprintf(stderr, "Error sending RETRIEVE\n");
        return NULL;
    }

    // TODO read answer
    char buffer[1025] = {0};
    int data_read = read(sockfd, buffer, 1024);
    if (data_read < 0)
    {
        fprintf(stderr, "Error retrieving header for retrieve\n");
        return NULL;
    }
    else if (data_read == 0)
    {
        fprintf(stderr, "EOF while retrieving header for retrieve\n");
        return NULL;
    }
    // printf("Retrieve resulted in %s\n", buffer);
    // Buffer should contain header
    if (strncmp("DATA ", buffer, 5) != 0)
    {
        if (buffer[0] == 'K' && buffer[1] == 'O')
        {
            printf("Retrieve %s got a KO ", name);
        }
        else
        {
            printf("Retrieve %s got %s", name, buffer);
        }
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
            printf("Recieved invalid object len");
            return NULL;
        }
        lenstr[i] = c;
    }
    lenstr[i] = '\0';
    int store_left = atoll(lenstr);
    int data_start = i + len_index + 3;
    if (store_left == 0)
    {
        return NULL; // TODO set result var
    }
    printf("Reading answer long %d\n", store_left);
    char *return_buffer = malloc(store_left + 1);
    return_buffer[0] = '\0';
    strcpy(return_buffer, buffer + data_start);
    int read_data = strlen(return_buffer);
    store_left -= read_data;
    int r = read_all(sockfd, return_buffer + read_data, store_left);
    if (r == -1)
    {
        fprintf(stderr, "Error retrieving data\n");
        free(return_buffer);
        return NULL;
    }
    else if (r == 0)
    {
        fprintf(stderr, "EOF retrieving data\n");
        sockfd = -1;
        free(return_buffer);
        return NULL;
    }
    return return_buffer;
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
    // TODO read answer
    char buffer[1024] = {0};
    read(sockfd, buffer, 1024);
    printf("Delete resulted in %s\n", buffer);
    return (buffer[0] == 'O' && buffer[1] == 'K');
}

int os_disconnect()
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return false;
    }
    dprintf(sockfd, "LEAVE \n");

    char buffer[1024] = {0};
    int err = read(sockfd, buffer, 1024);
    if (err < 0)
    {
        perror("Reading LEAVE answer");
        return false;
    }
    else if (err == 0)
    {
        fprintf(stderr, "EOF while reading LEAVE answer\n");
        return false;
    }
    printf("LEAVE resulted in %s\n", buffer);
    sockfd = -1;
    return (buffer[0] == 'O' && buffer[1] == 'K');
}