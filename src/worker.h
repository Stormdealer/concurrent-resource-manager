#ifndef WORKER_H
#define WORKER_H

#include "task_queue.h"
#include "../include/common.h"

typedef struct {
    int worker_id;
    TaskQueue *dispatch;             /* where the scheduler hands us tasks */
    TaskQueue *levels[MLFQ_LEVELS];  /* to requeue a task that isn't done yet */
    SharedStats *stats;
} WorkerArgs;

/* Thread entry point for one worker in the pool. On first touching a
 * task, blocks on resource_mgr_request() (Banker's algorithm gate)
 * before running. Runs the task for one MLFQ quantum; if the task's
 * burst isn't finished, demotes it a level and requeues it; otherwise
 * releases its resources and records completion stats. */
void *worker_run(void *arg);

#endif /* WORKER_H */
