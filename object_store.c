#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
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
        handle_error_n(err, "masking signals in main");
    }
    pthread_t signal_thread;
    int *spipe = malloc(sizeof(*spipe));
    if (spipe == NULL)
    { // TODO ERR
    }
    *spipe = signal_pipe;
    err = pthread_create(&signal_thread, NULL, handle_signals, spipe);
    if (err != 0)
    {
        handle_error_n(err, "creating signal thread");
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
    }
    *spipe = signal_pipe;
    err = pthread_create(&server_thread, NULL, handle_connections, spipe);
    if (err != 0)
    {
        handle_error_n(err, "creating server thread");
    }
    return server_thread;
}

int main(int argc, char *argv[])
{
    int err;
    printf("Starting object store...\n");
    printf("Process ID : %d\n", getpid());
    int signal_pipe[2];
    err = pipe(signal_pipe);
    if (err == -1)
    {
        perror("Creating pipe for signal handling\n");
    }
    pthread_t signal_thread = start_signal_handler(signal_pipe[1]);         // writing end
    pthread_t connection_thread = start_connection_handler(signal_pipe[0]); // reading end
    err = pthread_join(connection_thread, NULL);
    if (err != 0)
    {
        handle_error_n(err, "joining server thread")
    }
    err = pthread_join(signal_thread, NULL);
    if (err != 0)
    {
        handle_error_n(err, "joining signal thread")
    }
    return 0;
}