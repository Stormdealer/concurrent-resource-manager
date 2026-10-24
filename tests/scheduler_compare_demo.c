/* Optional standalone smoke-test harness for scheduler_compare.c.
 * Compile manually from the project root with:
 * gcc -Wall -Wextra -std=gnu11 -Isrc src/scheduler_compare.c tests/scheduler_compare_demo.c -o scheduler_compare_demo
 * Run: ./scheduler_compare_demo 25
 * This file lives in tests/, so it is not part of the normal src folder build. */
#include <stdio.h>
#include <stdlib.h>
#include "scheduler_compare.h"

#define RR_QUANTUM_MS 50

int main(int argc, char **argv)
{
    int task_count = 12;
    SchedulerCompareJob *jobs;
    SchedulerCompareResult results[SCHED_COMPARE_POLICY_COUNT];

    if (argc > 1) {
        char *end = NULL;
        long parsed = strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsed < 1 || parsed > 1024) {
            fprintf(stderr, "usage: %s [task_count: 1..1024]\n", argv[0]);
            return 1;
        }
        task_count = (int)parsed;
    }

    jobs = (SchedulerCompareJob *)malloc((size_t)task_count * sizeof(*jobs));
    if (jobs == NULL) {
        fprintf(stderr, "error: could not allocate workload\n");
        return 1;
    }

    if (!scheduler_compare_generate_workload(task_count, 42u, jobs) ||
        !scheduler_compare_run(jobs, task_count, RR_QUANTUM_MS, results)) {
        fprintf(stderr, "error: scheduler comparison failed\n");
        free(jobs);
        return 1;
    }

    printf("Scheduler-only comparison on %d reproducible jobs (seed 42)\n", task_count);
    scheduler_compare_print(results, RR_QUANTUM_MS);
    free(jobs);
    return 0;
}
