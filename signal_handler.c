#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"

static int handle_signal(int sigtype, int signalpipe)
{
    int w;
    switch (sigtype)
    {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        write(1, "Recieved termination signal, quitting...\n", 42);
        w = write(signalpipe, "T", 1);
        if (w == -1)
            perror("Error writing T to signal pipe");

        return false;
    case SIGUSR1:
        w = write(signalpipe, "S", 1);
        if (w == -1)
            perror("Error writing S to signal pipe");
        write(STDOUT_FILENO, "Recieved SIGUSR1\n", 18);
        return true;
    case SIGPIPE:
        write(STDERR_FILENO, "Received signal SIGPIPE!\n", 26);
        return true;
    }
    write(STDOUT_FILENO, "Unhandled signal, quitting...\n", 31);
    return false;
}

void *handle_signals(void *spipe)
{
    int signalpipe = *((int *)spipe);
    int sig;
    int err;
    sigset_t sigmask;
    do
    {
        err = sigemptyset(&sigmask);
        err |= sigaddset(&sigmask, SIGHUP);
        err |= sigaddset(&sigmask, SIGTERM);
        err |= sigaddset(&sigmask, SIGINT);
        err |= sigaddset(&sigmask, SIGUSR1);
        err |= sigaddset(&sigmask, SIGPIPE);
        if (err != 0)
        {
            print_error_n(err, "error setting signal handler mask\n");
            sig = SIGTERM;
        }
        else
        {
            err = sigwait(&sigmask, &sig);
            if (err != 0)
            {
                print_error_n(err, "calling sigwait in signal handler\n");
                sig = SIGTERM;
            }
        }
    } while (handle_signal(sig, signalpipe));
    close(signalpipe);
    free(spipe);
    return NULL;
}