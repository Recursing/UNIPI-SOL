#ifndef CONN_HAND_H
#define CONN_HAND_H
#include <pthread.h>

// Create socket and handle connections
void *handle_connections(void *pipe);

extern int active_workers;
extern pthread_mutex_t active_workers_lock;
extern pthread_cond_t active_workers_CV;

#endif