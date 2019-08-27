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
        fprintf(stderr, "Invalid name\n");
        return false;
    }
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
        perror("Connection Failed");
        return false;
    }
    printf("connected to socket\n");
    dprintf(sockfd, "R");
    // sleep(1);
    dprintf(sockfd, "E");
    sleep(1);
    dprintf(sockfd, "G");
    // sleep(1);
    dprintf(sockfd, "I");
    sleep(1);
    dprintf(sockfd, "STER %s ", name);
    // sleep(1);
    dprintf(sockfd, "\n");
    printf("Register message sent\n");
    // TODO read answer

    char buffer[1024] = {0};
    read(sockfd, buffer, 1024);
    printf("%s\n", buffer);
    return 0;
}

int os_store(char *name, void *block, size_t len)
{
    if (sockfd == -1)
    {
        fprintf(stderr, "Not connected\n");
        return 0;
    }
    if (name == NULL || strlen(name) < 1 || strlen(name) > MAX_FILENAME_LEN)
    {
        fprintf(stderr, "Invalid name\n");
        return false;
    }
    dprintf(sockfd, "STORE %s %lu \n %s", name, len + 1, (char *)block);
    sleep(2);
    dprintf(sockfd, "E");
    // TODO read answer
    char buffer[1024] = {0};
    read(sockfd, buffer, 1024);
    printf("%s\n", buffer);
    return 0;
}
