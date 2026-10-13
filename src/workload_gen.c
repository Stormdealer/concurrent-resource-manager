#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "workload_gen.h"

static int rand_range(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

static Task *make_task(int id) {
    Task *t = calloc(1, sizeof(Task));
    t->id = id;
    t->type = rand_range(0, 2);

    switch (t->type) {
        case TASK_CPU_BOUND:
            t->total_burst_ms = rand_range(150, 400);
            t->max_need[RES_CPU_CORES]   = rand_range(1, 2);
            t->max_need[RES_IO_CHANNELS] = 0;
            t->max_need[RES_MEM_BLOCKS]  = rand_range(1, 3);
            break;
        case TASK_IO_BOUND:
            t->total_burst_ms = rand_range(30, 100);
            t->max_need[RES_CPU_CORES]   = 1;
            t->max_need[RES_IO_CHANNELS] = 1;
            t->max_need[RES_MEM_BLOCKS]  = rand_range(1, 2);
            break;
        case TASK_MIXED:
        default:
            t->total_burst_ms = rand_range(80, 250);
            t->max_need[RES_CPU_CORES]   = 1;
            t->max_need[RES_IO_CHANNELS] = rand_range(0, 1);
            t->max_need[RES_MEM_BLOCKS]  = rand_range(1, 4);
            break;
    }
    /* Defer one unit of any resource dimension where the task needs more
     * than 1 unit -- this makes the task request that extra unit partway
     * through execution instead of all at once, so it can be genuinely
     * holding some resources while waiting for more (hold-and-wait). */
    int any_deferred = 0;
    for (int j = 0; j < NUM_RESOURCES; j++) {
        if (t->max_need[j] > 1) {
            t->need1[j] = t->max_need[j] - 1;
            any_deferred = 1;
        } else {
            t->need1[j] = t->max_need[j];
        }
    }
    t->phase2_done = !any_deferred; /* nothing deferred -> no top-up needed */

    t->burst_ms = t->total_burst_ms;
    t->queue_level = 0; /* everyone enters at the highest MLFQ priority */
    t->started = 0;
    clock_gettime(CLOCK_MONOTONIC, &t->arrival_time);
    return t;
}

void *workload_gen_run(void *arg) {
    WorkloadGenArgs *args = (WorkloadGenArgs *)arg;

    for (int i = 1; i <= args->num_tasks; i++) {
        Task *t = make_task(i);
        task_queue_push(args->incoming, t);
        printf("[generator] task %2d arrived  (type=%d, burst=%dms, "
               "need=[cpu:%d io:%d mem:%d])\n",
               t->id, t->type, t->total_burst_ms,
               t->max_need[RES_CPU_CORES], t->max_need[RES_IO_CHANNELS],
               t->max_need[RES_MEM_BLOCKS]);

        if (args->max_spawn_gap_ms > 0) {
            usleep(rand_range(1, args->max_spawn_gap_ms) * 1000);
        }
    }
    return NULL;
}
