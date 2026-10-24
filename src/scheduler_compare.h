#ifndef SCHEDULER_COMPARE_H
#define SCHEDULER_COMPARE_H

/* Scheduler-only comparison module for the Concurrent Resource Manager.
 * It compares FCFS, Round Robin and MLFQ on the SAME supplied workload.
 * Resource allocation and the three-worker concurrency are intentionally
 * excluded from this isolated CPU-scheduling comparison. */

typedef struct {
    int id;
    int arrival_ms;
    int burst_ms;
} SchedulerCompareJob;

typedef struct {
    double average_waiting_ms;
    double average_turnaround_ms;
    int total_elapsed_ms;
} SchedulerCompareResult;

typedef enum {
    SCHED_COMPARE_FCFS = 0,
    SCHED_COMPARE_ROUND_ROBIN = 1,
    SCHED_COMPARE_MLFQ = 2,
    SCHED_COMPARE_POLICY_COUNT = 3
} SchedulerComparePolicy;

/* Creates a reproducible workload with the same broad CPU/I/O/mixed burst
 * ranges used in the project. The caller owns the supplied jobs array. */
int scheduler_compare_generate_workload(int task_count,
                                        unsigned int seed,
                                        SchedulerCompareJob *jobs);

/* Runs all three policies over the same jobs array. Jobs must have
 * non-negative arrival times and positive burst times. rr_quantum_ms must
 * be positive. Returns 1 on success, 0 on invalid input/allocation failure. */
int scheduler_compare_run(const SchedulerCompareJob *jobs,
                          int task_count,
                          int rr_quantum_ms,
                          SchedulerCompareResult results[SCHED_COMPARE_POLICY_COUNT]);

void scheduler_compare_print(const SchedulerCompareResult results[SCHED_COMPARE_POLICY_COUNT],
                             int rr_quantum_ms);

#endif /* SCHEDULER_COMPARE_H */
