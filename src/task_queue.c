
#include <stdlib.h>
#include "task_queue.h"

void task_queue_init(TaskQueue *q) {
    q->head = q->tail = NULL;
    q->count = 0;
    q->shutdown = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void task_queue_push(TaskQueue *q, Task *t) {
    pthread_mutex_lock(&q->lock);
    t->next = NULL;

    if (q->tail) {
        q->tail->next = t;
    } else {
        q->head = t;
    }

    q->tail = t;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

Task *task_queue_pop(TaskQueue *q) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    Task *t = q->head;
    q->head = t->next;

    if (!q->head) {
        q->tail = NULL;
    }

    q->count--;
    t->next = NULL;

    pthread_mutex_unlock(&q->lock);
    return t;
}

/* NEW: Remove a task immediately without waiting if the queue is empty.
   Returns NULL when no task is available. */
Task *task_queue_try_pop(TaskQueue *q) {
    pthread_mutex_lock(&q->lock);

    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    Task *t = q->head;
    q->head = t->next;

    if (!q->head) {
        q->tail = NULL;
    }

    q->count--;
    t->next = NULL;

    pthread_mutex_unlock(&q->lock);
    return t;
}

int task_queue_size(TaskQueue *q) {
    pthread_mutex_lock(&q->lock);
    int n = q->count;
    pthread_mutex_unlock(&q->lock);
    return n;
}

void task_queue_shutdown(TaskQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

void task_queue_destroy(TaskQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
}
