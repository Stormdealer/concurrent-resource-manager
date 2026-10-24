#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "scheduler.h"
#include "mlfq_boost.h"
#include <unistd.h>
#include <time.h>
#include "scheduler.h"
#include "mlfq_boost.h"

const int LEVEL_QUANTUM_MS[MLFQ_LEVELS] = {20, 50, 100};

#define MLFQ_BOOST_INTERVAL_MS 1000

static long long current_time_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((long long)ts.tv_sec * 1000LL) +
           (ts.tv_nsec / 1000000LL);
}

void *scheduler_run(void *arg) {
    SchedulerArgs *s = (SchedulerArgs *)arg;

    long long last_boost_time = current_time_ms();

    while (*s->system_running) {
        Task *chosen = NULL;
        long long now = current_time_ms();

        /* Periodically promote waiting lower-priority tasks. */
        if (now - last_boost_time >= MLFQ_BOOST_INTERVAL_MS) {
            int promoted = mlfq_priority_boost(
                s->levels[0],
                s->levels[1],
                s->levels[2]
            );

            if (promoted > 0) {
                printf("[scheduler] priority boost: %d tasks promoted\n",
                       promoted);
            }

            last_boost_time = now;
        }

        /* Strict priority: only use a lower level if all higher
         * levels are currently empty. */
        for (int level = 0;
             level < MLFQ_LEVELS && !chosen;
             level++) {

            if (task_queue_size(s->levels[level]) > 0) {
                chosen = task_queue_try_pop(s->levels[level]);
            }
        }

        if (chosen) {
            if (chosen->queue_level < 0 ||
                chosen->queue_level >= MLFQ_LEVELS) {
                chosen->queue_level = MLFQ_LEVELS - 1;
            }

            task_queue_push(s->dispatch, chosen);
        } else {
            usleep(5000);
        }
    }

    /* Stop workers after the scheduler finishes dispatching. */
    task_queue_shutdown(s->dispatch);

    return NULL;
}
