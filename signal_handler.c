#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"

// Write signal code to pipe
// Returns true if thread should keep listening, false if it should exit
static int handle_signal(int sigtype, int signalpipe)
{
    int write_res;
    switch (sigtype)
    {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        write_res = write(signalpipe, "T", 1);
        if (write_res == -1)
            perror("Error writing T to signal pipe");
        write_res = write(1, "Recieved termination signal, quitting...\n", 41);
        return false;

    case SIGUSR1:
        write_res = write(signalpipe, "S", 1);
        if (write_res == -1)
            perror("Error writing S to signal pipe");
        write_res = write(STDOUT_FILENO, "Recieved SIGUSR1\n", 18);
        return true;

    case SIGPIPE:
        write_res = write(STDERR_FILENO, "Received signal SIGPIPE!\n", 25);
        return true;
    }
    write_res = write(STDOUT_FILENO, "Unhandled signal, quitting...\n", 30);
    if (write_res == -1)
        perror("Error writing signal handling to STDOUT or STDERR");
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