#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>
#include "../include/common.h"

/* A simple thread-safe FIFO queue of Task*.
 * Protected by a mutex; a condition variable lets consumers block
 * until a producer signals that a new task is available. */
typedef struct {
    Task *head;
    Task *tail;
    int   count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    int shutdown; /* set to 1 to wake all waiters and stop the system */
} TaskQueue;

void task_queue_init(TaskQueue *q);
void task_queue_push(TaskQueue *q, Task *t);

/* Blocks until a task is available or the queue is shut down.
 * Returns NULL if shutting down with an empty queue. */
Task *task_queue_pop(TaskQueue *q);

int  task_queue_size(TaskQueue *q);
void task_queue_shutdown(TaskQueue *q);
void task_queue_destroy(TaskQueue *q);

#endif /* TASK_QUEUE_H */
