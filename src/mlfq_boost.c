#include "mlfq_boost.h"

/* Promote waiting tasks from the lower-priority queues. */
int mlfq_priority_boost(TaskQueue *level0,
                        TaskQueue *level1,
                        TaskQueue *level2)
{
    Task *task;
    int promoted = 0;

    if (level0 == NULL || level1 == NULL || level2 == NULL ||
        level0 == level1 || level0 == level2 || level1 == level2) {
        return -1;
    }

    /* Drain level 1 in FIFO order and append to level 0. */
    while ((task = task_queue_try_pop(level1)) != NULL) {
        task->queue_level = 0;
        task_queue_push(level0, task);
        promoted++;
    }

    /* Then drain level 2 in FIFO order and append to level 0. */
    while ((task = task_queue_try_pop(level2)) != NULL) {
        task->queue_level = 0;
        task_queue_push(level0, task);
        promoted++;
    }

    return promoted;
}
