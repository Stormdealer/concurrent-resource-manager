#include <unistd.h>
#include "scheduler.h"

const int LEVEL_QUANTUM_MS[MLFQ_LEVELS] = {20, 50, 100};

void *scheduler_run(void *arg) {
    SchedulerArgs *s = (SchedulerArgs *)arg;

    while (*s->system_running) {
        Task *chosen = NULL;

        /* Strict priority: only look at a lower level if every higher
         * level is currently empty. */
        for (int level = 0; level < MLFQ_LEVELS && !chosen; level++) {
            if (task_queue_size(s->levels[level]) > 0) {
                chosen = task_queue_pop(s->levels[level]);
            }
        }

        if (chosen) {
            chosen->queue_level = chosen->queue_level < MLFQ_LEVELS
                                       ? chosen->queue_level : MLFQ_LEVELS - 1;
            task_queue_push(s->dispatch, chosen);
        } else {
            usleep(5000); /* nothing ready right now; avoid busy-spinning */
        }
    }

    /* No more tasks will ever be scheduled: let worker threads drain
     * the dispatch queue and then exit their pop loop. */
    task_queue_shutdown(s->dispatch);
    return NULL;
}
