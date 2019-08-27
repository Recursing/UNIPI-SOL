#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "server_worker.h"
#include "utils.h"

static void print_stats()
{
    printf("stats placeholder!\n");
}

static int handle_signal(int signalpipe)
{
    int is_done = false;
    char *sigtype = malloc(1);
    int r = read(signalpipe, sigtype, 1);
    if (r == -1)
    {
        perror("reading from signal pipe");
    }
    else if (r == 0)
    {
        printf("Signal handler closed before sending T");
        is_done = true;
    }
    else
    {
        switch (*sigtype)
        {
        case 'T':
            is_done = true;
            break;
        case 'S':
            print_stats();
            break;
        }
    }
    free(sigtype);
    return is_done;
}

static void handle_connect(int socked_fd, int broadcast_termination_pipe)
{
    int new_socket;
    new_socket = accept(socked_fd, NULL, NULL);
    if (new_socket == -1)
    {
        perror("Accepting connection to socket");
        exit(EXIT_FAILURE);
    }
    pthread_t worker_thread;
    int *fds = malloc(2 * sizeof(new_socket)); // will be freed by worker
    fds[0] = broadcast_termination_pipe;
    fds[1] = new_socket;

    pthread_create(&worker_thread, NULL, start_worker, (void *)fds);
    pthread_detach(worker_thread); // So that valgrind doesn't complain and I don't have to join
}

static void dispatcher_loop(int signalpipe, int socked_fd)
{
    int is_done = false;
    int err;

    int broadcast_termination_pipe[2];
    err = pipe(broadcast_termination_pipe);
    if (err == -1)
    {
        perror("Creating pipe for broadcasting termination\n");
    }

    struct pollfd pfds[2];
    pfds[0].fd = signalpipe;
    pfds[0].events = POLLIN;
    pfds[1].fd = socked_fd;
    pfds[1].events = POLLIN;
    while (!is_done)
    {
        err = poll(pfds, 2, -1);
        if (err == -1)
        {
            perror("Polling signals and socket");
        }
        if (pfds[0].revents & POLLIN)
        {
            is_done = handle_signal(signalpipe);
        }
        if (pfds[1].revents & POLLIN)
        {
            handle_connect(socked_fd, broadcast_termination_pipe[0]);
        }
        if (pfds[1].revents == POLLHUP)
        {
            perror("socket POLLHUP");
            exit(EXIT_FAILURE);
        }
    }
    close(signalpipe);
    write(broadcast_termination_pipe[1], "T", 1);
    // TODO wait for all threads to close
    close(broadcast_termination_pipe[0]);
    close(broadcast_termination_pipe[1]);
}

static int create_socket()
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Error opening socket");
        return -1;
    }
    int err;
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKPATH);
    err = bind(fd, (struct sockaddr *)&address, sizeof(address));
    if (err == -1)
    {
        perror("Error binding socket");
        return -1;
    }
    err = listen(fd, SOMAXCONN);
    if (err == -1)
    {
        perror("Error listening to socket");
        return -1;
    }
    return fd;
}

void *handle_connections(void *spipe)
{
    int signalpipe = *((int *)spipe);
    int socket_fd = create_socket();
    if (socket_fd == -1)
    {
        exit(EXIT_FAILURE);
    }
    dispatcher_loop(signalpipe, socket_fd);
    int err;
    err = unlink(SOCKPATH);
    if (err == -1)
    {
        perror("unlinking socket");
    }
    free(spipe);
    return NULL;
}
