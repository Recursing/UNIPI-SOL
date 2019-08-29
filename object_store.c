#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include "utils.h"
#include "signal_handler.h"
#include "connection_handler.h"

static pthread_t start_signal_handler(int signal_pipe)
{
    int err;
    // Ignore all signals
    sigset_t blocked_signals;
    err = sigfillset(&blocked_signals);
    if (err != 0)
    {
        fprintf(stderr, "filling signal set\n");
    }
    err = pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);
    if (err != 0)
    {
        print_error_n(err, "masking signals in main");
        return 0; // 0 cannot be a valid thread id
    }
    pthread_t signal_thread;
    int *spipe = malloc(sizeof(*spipe));
    if (spipe == NULL)
    {
        fprintf(stderr, "cannot malloc");
        return 0; // 0 cannot be a valid thread id
    }
    *spipe = signal_pipe;
    err = pthread_create(&signal_thread, NULL, handle_signals, spipe);
    if (err != 0)
    {
        print_error_n(err, "creating signal thread");
        return 0; // 0 cannot be a valid thread id
    }
    return signal_thread;
}

static pthread_t start_connection_handler(int signal_pipe)
{
    int err;
    pthread_t server_thread;
    int *spipe = malloc(sizeof(*spipe)); // will be freed by thread
    if (spipe == NULL)
    {
        perror("Can't malloc");
        return 0; // 0 cannot be a valid thread id
    }
    *spipe = signal_pipe;
    err = pthread_create(&server_thread, NULL, handle_connections, spipe);
    if (err != 0)
    {
        print_error_n(err, "creating server thread");
        return 0; // 0 cannot be a valid thread id
    }
    return server_thread;
}

int main(int argc, char *argv[])
{
    int err;
    printf("Starting object store...\n");
    printf("Process ID : %d\n", getpid());

    err = mkdir(STORAGEPATH, 0700);
    if (err < 0 && errno != EEXIST)
    {
        fprintf(stderr, "Error creating storage folder\n");
        return 1;
    }
    int signal_pipe[2];
    err = pipe(signal_pipe);
    if (err == -1)
    {
        perror("Creating pipe for signal handling\n");
        return 1;
    }
    pthread_t signal_thread = start_signal_handler(signal_pipe[1]); // writing end
    if (signal_thread == 0)
    {
        return 1;
    }
    pthread_t connection_thread = start_connection_handler(signal_pipe[0]); // reading end
    if (connection_thread == 0)
    {
        kill(getpid(), SIGTERM); // Wake up and terminate signal handler
    }
    else
    {
        err = pthread_join(connection_thread, NULL);
        if (err != 0)
        {
            print_error_n(err, "joining server thread");
            kill(getpid(), SIGTERM); // Wake up and terminate signal handler
        }
    }
    err = pthread_join(signal_thread, NULL);
    if (err != 0)
    {
        print_error_n(err, "joining signal thread");
        return 1;
    }
    return 0;
}