#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task_queue.h"

/* Time slice (ms) granted at each MLFQ level. Level 0 = shortest slice
 * / highest priority, favors short interactive/IO-bound bursts.
 * Level (MLFQ_LEVELS-1) = longest slice, for CPU-bound batch work. */
extern const int LEVEL_QUANTUM_MS[MLFQ_LEVELS];

typedef struct {
    TaskQueue *levels[MLFQ_LEVELS]; /* one queue per priority level */
    TaskQueue *dispatch;            /* hand-off queue read by the worker pool */
    volatile int *system_running;   /* main sets this to 0 to stop the loop */
} SchedulerArgs;

/* Thread entry point. Repeatedly picks the highest non-empty priority
 * level (round-robin within a level happens naturally via the queue's
 * FIFO order) and hands the chosen task to the worker pool. Shuts the
 * dispatch queue down once system_running goes false, so worker
 * threads can exit cleanly. */
void *scheduler_run(void *arg);

#endif /* SCHEDULER_H */
