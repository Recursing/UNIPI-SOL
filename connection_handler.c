#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "server_worker.h"
#include "utils.h"
#include "stats.h"

// Using shared memory to count active workers
// Needed so that the handler doesn't need to join all of them explicitly
// And can just wait for active_workers to be set to 0
// Without keeping a list of thread_ids in a data structure
int active_workers = 0;
pthread_mutex_t active_workers_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t active_workers_CV = PTHREAD_COND_INITIALIZER;

// Create and start a detached thread to gather and print statistics
static void handle_stats()
{
    pthread_t stats_thread;
    int err = pthread_create(&stats_thread, NULL, print_stats, NULL);
    if (err != 0)
    {
        print_error_n(err, "creating stats thread");
    }
    err = pthread_detach(stats_thread); // So that I don't have to join it
    if (err != 0)
    {
        print_error_n(err, "detatching stats thread");
    }
}

// Read and handle signal recieved via the signal pipe
// Return true if it's the connection handler should terminate, false otherwise
static int handle_signal(int signalpipe)
{
    int is_done = false;
    char *sigtype = malloc(1);
    int r = read(signalpipe, sigtype, 1);
    if (r == -1)
    {
        perror("Error reading from signal pipe, assuming T");
        is_done = true;
    }
    else if (r == 0)
    {
        fprintf(stderr, "Signal handler closed before sending T\n");
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
            handle_stats();
            break;
        }
    }
    free(sigtype);
    return is_done;
}

// Handle a new connection: create a new socket fd, create and start a new worker
static void handle_connect(int socked_fd, int broadcast_termination_pipe)
{
    int new_socket;
    new_socket = accept(socked_fd, NULL, NULL);
    if (new_socket == -1)
    {
        fprintf(stderr, "ERROR with %d workers active\n", active_workers);
        perror("Accepting connection to socket. SENDING TERM");
        kill(getpid(), SIGTERM); // Wake up and terminate signal handler
        return;
    }

    pthread_t worker_thread;
    int *fds = calloc(2, sizeof(new_socket)); // will be freed by worker
    fds[0] = broadcast_termination_pipe;
    fds[1] = new_socket;

    int err = pthread_create(&worker_thread, NULL, start_worker, (void *)fds);
    if (err != 0)
    {
        print_error_n(err, "creating worker thread");
        fprintf(stderr, "ERROR creating worker thread. SENDING TERM\n");
        kill(getpid(), SIGTERM); // Wake up and terminate signal handler
    }
    err = pthread_detach(worker_thread); // So that I don't have to join it
    if (err != 0)
    {
        print_error_n(err, "detatching worker thread");
    }
}

// Main loop, poll on signal pipe and socket
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
        // printf("Polled something! %d %d\n", pfds[0].revents, pfds[1].revents);
        if (err == -1)
        {
            perror("Polling signals and socket");
        }
        if (pfds[0].revents & POLLIN)
        {
            is_done = handle_signal(signalpipe);
        }
        else if (pfds[1].revents & POLLIN)
        {
            handle_connect(socked_fd, broadcast_termination_pipe[0]);
        }
        else if (pfds[1].revents == POLLHUP)
        {
            perror("socket POLLHUP");
            is_done = true;
        }
    }
    close(signalpipe);

    // Send termination byte to all workers
    // They will not read it but will recieve POLLIN on their end of the pipe and terminate
    int write_err = write(broadcast_termination_pipe[1], "T", 1);
    if (write_err < 0)
    {
        perror("Writing termination byte to broadcast_termination_pipe");
    }

    // Wait for all workers to exit
    pthread_mutex_lock(&active_workers_lock);
    while (active_workers > 0)
        pthread_cond_wait(&active_workers_CV, &active_workers_lock);
    pthread_mutex_unlock(&active_workers_lock);

    close(broadcast_termination_pipe[0]);
    close(broadcast_termination_pipe[1]);
}

// Create the objstore.sock socket
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
        perror("Error binding socket to objstore.sock");
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

// Thread entry point
void *handle_connections(void *spipe)
{
    int signalpipe = *((int *)spipe);
    int socket_fd = create_socket();
    if (socket_fd == -1)
    {
        free(spipe);
        kill(getpid(), SIGTERM); // Wake up and terminate signal handler
        return NULL;
    }

    dispatcher_loop(signalpipe, socket_fd);

    int err;
    err = unlink(SOCKPATH); // Delete socket
    if (err == -1)
    {
        perror("unlinking socket");
    }

    close(socket_fd);
    free(spipe);
    return NULL;
}
