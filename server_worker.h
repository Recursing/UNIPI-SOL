#ifndef SER_WORK_H
#define SER_WORK_H

// fds[0] is a pipe fd for recieving a termination signal
// fds[1] is the socket fd for handling client communication
void *start_worker(void *fds);

#endif