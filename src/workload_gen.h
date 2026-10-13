#ifndef WORKLOAD_GEN_H
#define WORKLOAD_GEN_H

#include "task_queue.h"

typedef struct {
    TaskQueue *incoming;   /* where generated tasks are pushed */
    int num_tasks;         /* how many tasks to generate */
    int max_spawn_gap_ms;  /* random delay between spawns, simulates arrivals */
} WorkloadGenArgs;

/* Thread entry point: generates args->num_tasks heterogeneous tasks
 * (CPU-bound / IO-bound / mixed) with random bursts and random
 * resource claims, pushing each into the incoming queue. */
void *workload_gen_run(void *arg);

#endif /* WORKLOAD_GEN_H */
