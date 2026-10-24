
#include "scheduler_compare.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MLFQ_LEVELS 3
static const int MLFQ_QUANTUM_MS[MLFQ_LEVELS] = {20, 50, 100};
#define DEFAULT_RR_QUANTUM_MS 50

typedef struct {
    int id;
    int arrival_ms;
    int burst_ms;
    int remaining_ms;
    int completion_ms;
    int queue_level;
} JobState;

typedef struct {
    int *items;
    int capacity;
    int head;
    int count;
} IndexQueue;

static unsigned int next_random(unsigned int *state)
{
    /* Small deterministic PRNG. It does not alter the project's global rand(). */
    *state = (*state * 1103515245u) + 12345u;
    return (*state / 65536u) % 32768u;
}

static int random_range(unsigned int *state, int low, int high)
{
    return low + (int)(next_random(state) % (unsigned int)(high - low + 1));
}

int scheduler_compare_generate_workload(int task_count,
                                        unsigned int seed,
                                        SchedulerCompareJob *jobs)
{
    unsigned int state = seed;
    int arrival = 0;
    int i;

    if (jobs == NULL || task_count < 1) {
        return 0;
    }
    if (state == 0u) {
        state = 1u;
    }

    for (i = 0; i < task_count; i++) {
        int type = random_range(&state, 0, 2);
        int burst;

        if (type == 0) {
            burst = random_range(&state, 150, 400); /* CPU-bound */
        } else if (type == 1) {
            burst = random_range(&state, 30, 100);  /* I/O-bound */
        } else {
            burst = random_range(&state, 80, 250);  /* mixed */
        }

        if (i > 0) {
            arrival += random_range(&state, 1, 40);
        }
        jobs[i].id = i + 1;
        jobs[i].arrival_ms = arrival;
        jobs[i].burst_ms = burst;
    }
    return 1;
}

static int queue_init(IndexQueue *queue, int capacity)
{
    queue->items = (int *)malloc((size_t)capacity * sizeof(int));
    if (queue->items == NULL) {
        return 0;
    }
    queue->capacity = capacity;
    queue->head = 0;
    queue->count = 0;
    return 1;
}

static void queue_destroy(IndexQueue *queue)
{
    free(queue->items);
    queue->items = NULL;
    queue->capacity = 0;
    queue->head = 0;
    queue->count = 0;
}

static int queue_push(IndexQueue *queue, int value)
{
    int tail;
    if (queue->count >= queue->capacity) {
        return 0;
    }
    tail = (queue->head + queue->count) % queue->capacity;
    queue->items[tail] = value;
    queue->count++;
    return 1;
}

static int queue_pop(IndexQueue *queue, int *value)
{
    if (queue->count == 0) {
        return 0;
    }
    *value = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return 1;
}

static int validate_jobs(const SchedulerCompareJob *jobs, int task_count)
{
    int i;
    if (jobs == NULL || task_count < 1) {
        return 0;
    }
    for (i = 0; i < task_count; i++) {
        if (jobs[i].arrival_ms < 0 || jobs[i].burst_ms <= 0) {
            return 0;
        }
        if (i > 0 && jobs[i].arrival_ms < jobs[i - 1].arrival_ms) {
            return 0; /* jobs must be ordered by non-decreasing arrival time */
        }
    }
    return 1;
}

static int run_one_policy(const SchedulerCompareJob *jobs,
                          int task_count,
                          SchedulerComparePolicy policy,
                          int rr_quantum_ms,
                          SchedulerCompareResult *result)
{
    JobState *state = NULL;
    IndexQueue queues[MLFQ_LEVELS] = {{0}};
    int queue_count = (policy == SCHED_COMPARE_MLFQ) ? MLFQ_LEVELS : 1;
    int next_arrival = 0;
    int completed = 0;
    int now = 0;
    long total_waiting = 0;
    long total_turnaround = 0;
    int ok = 0;
    int i;

    state = (JobState *)calloc((size_t)task_count, sizeof(JobState));
    if (state == NULL) {
        return 0;
    }

    for (i = 0; i < task_count; i++) {
        state[i].id = jobs[i].id;
        state[i].arrival_ms = jobs[i].arrival_ms;
        state[i].burst_ms = jobs[i].burst_ms;
        state[i].remaining_ms = jobs[i].burst_ms;
        state[i].completion_ms = 0;
        state[i].queue_level = 0;
    }

    for (i = 0; i < queue_count; i++) {
        if (!queue_init(&queues[i], task_count + 1)) {
            goto cleanup;
        }
    }

    while (completed < task_count) {
        int chosen = -1;
        int level = 0;

        /* New arrivals enter the highest ready level. */
        while (next_arrival < task_count &&
               state[next_arrival].arrival_ms <= now) {
            if (!queue_push(&queues[0], next_arrival)) {
                goto cleanup;
            }
            next_arrival++;
        }

        if (policy == SCHED_COMPARE_MLFQ) {
            for (level = 0; level < queue_count; level++) {
                if (queue_pop(&queues[level], &chosen)) {
                    break;
                }
            }
        } else {
            if (queue_pop(&queues[0], &chosen)) {
                level = 0;
            }
        }

        if (chosen < 0) {
            if (next_arrival >= task_count) {
                goto cleanup; /* impossible state: unfinished jobs but no ready/arriving jobs */
            }
            now = state[next_arrival].arrival_ms;
            continue;
        }

        {
            int slice;
            int ran;

            if (policy == SCHED_COMPARE_FCFS) {
                slice = state[chosen].remaining_ms; /* non-preemptive */
            } else if (policy == SCHED_COMPARE_ROUND_ROBIN) {
                slice = rr_quantum_ms;
            } else {
                slice = MLFQ_QUANTUM_MS[level];
            }

            ran = (state[chosen].remaining_ms < slice)
                      ? state[chosen].remaining_ms : slice;

            now += ran;
            state[chosen].remaining_ms -= ran;

            /* Arrivals during a time slice join the ready queue before the
             * unfinished running job is requeued, as in normal RR/MLFQ. */
            while (next_arrival < task_count &&
                   state[next_arrival].arrival_ms <= now) {
                if (!queue_push(&queues[0], next_arrival)) {
                    goto cleanup;
                }
                next_arrival++;
            }

            if (state[chosen].remaining_ms == 0) {
                long turnaround;
                long waiting;

                state[chosen].completion_ms = now;
                turnaround = (long)now - state[chosen].arrival_ms;
                waiting = turnaround - state[chosen].burst_ms;

                if (waiting < 0) {
                    waiting = 0;
                }

                total_waiting += waiting;
                total_turnaround += turnaround;
                completed++;
            } else {
                if (policy == SCHED_COMPARE_MLFQ) {
                    /* Demote a task until the lowest level, then keep it there. */
                    if (level < MLFQ_LEVELS - 1) {
                        state[chosen].queue_level = level + 1;
                    } else {
                        state[chosen].queue_level = level;
                    }

                    if (!queue_push(
                            &queues[state[chosen].queue_level], chosen)) {
                        goto cleanup;
                    }
                } else {
                    /* FCFS and Round Robin use the single ready queue. */
                    if (!queue_push(&queues[0], chosen)) {
                        goto cleanup;
                    }
                }
            }
        }
    }

    result->average_waiting_ms =
        (double)total_waiting / (double)task_count;
    result->average_turnaround_ms =
        (double)total_turnaround / (double)task_count;
    result->total_elapsed_ms = now;

    ok = 1;

cleanup:
    for (i = 0; i < queue_count; i++) {
        queue_destroy(&queues[i]);
    }

    free(state);
    return ok;
}

int scheduler_compare_run(const SchedulerCompareJob *jobs,
                          int task_count,
                          int rr_quantum_ms,
                          SchedulerCompareResult results[SCHED_COMPARE_POLICY_COUNT])
{
    int policy;

    if (!validate_jobs(jobs, task_count) ||
        results == NULL ||
        rr_quantum_ms <= 0) {
        return 0;
    }

    for (policy = 0; policy < SCHED_COMPARE_POLICY_COUNT; policy++) {
        if (!run_one_policy(jobs,
                            task_count,
                            (SchedulerComparePolicy)policy,
                            rr_quantum_ms,
                            &results[policy])) {
            return 0;
        }
    }

    return 1;
}

void scheduler_compare_print(
    const SchedulerCompareResult results[SCHED_COMPARE_POLICY_COUNT],
    int rr_quantum_ms)
{
    if (results == NULL || rr_quantum_ms <= 0) {
        return;
    }

    printf("\n===== CPU scheduling comparison (single CPU; resources excluded) =====\n");
    printf("Policy              Avg waiting (ms)   Avg turnaround (ms)   Elapsed (ms)\n");

    printf("FCFS                %16.2f   %20.2f   %12d\n",
           results[SCHED_COMPARE_FCFS].average_waiting_ms,
           results[SCHED_COMPARE_FCFS].average_turnaround_ms,
           results[SCHED_COMPARE_FCFS].total_elapsed_ms);

    printf("Round Robin (q=%d)  %16.2f   %20.2f   %12d\n",
           rr_quantum_ms,
           results[SCHED_COMPARE_ROUND_ROBIN].average_waiting_ms,
           results[SCHED_COMPARE_ROUND_ROBIN].average_turnaround_ms,
           results[SCHED_COMPARE_ROUND_ROBIN].total_elapsed_ms);

    printf("MLFQ (20/50/100)     %16.2f   %20.2f   %12d\n",
           results[SCHED_COMPARE_MLFQ].average_waiting_ms,
           results[SCHED_COMPARE_MLFQ].average_turnaround_ms,
           results[SCHED_COMPARE_MLFQ].total_elapsed_ms);

    printf("Note: this isolates CPU scheduling; it does not include the project's\n");
    printf("three-worker concurrency or Banker's Algorithm resource contention.\n");
}
