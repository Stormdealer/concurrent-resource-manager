#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "task_queue.h"
#include "resource_mgr.h"
#include "workload_gen.h"
#include "scheduler.h"
#include "worker.h"
#include "stats.h"
#include "monitor_ipc.h"

#define NUM_WORKERS      3
#define SHM_NAME         "/crm_stats"
#define SEM_NAME         "/crm_ready"

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0); /* line-buffered even when piped/tee'd, so fork() can't duplicate buffered output */
    int num_tasks = (argc > 1) ? atoi(argv[1]) : 12;
    if (num_tasks < 1 || num_tasks > MAX_TASKS) {
        fprintf(stderr, "usage: %s [num_tasks: 1..%d]\n", argv[0], MAX_TASKS);
        return 1;
    }
    srand(42); /* fixed seed: reproducible runs for grading/demo */

    resource_mgr_init();
    SharedStats *stats = monitor_shared_create(SHM_NAME);
    pid_t monitor_pid = monitor_spawn(stats, SEM_NAME, 200, num_tasks);

    TaskQueue levels[MLFQ_LEVELS];
    TaskQueue dispatch;
    for (int i = 0; i < MLFQ_LEVELS; i++) task_queue_init(&levels[i]);
    task_queue_init(&dispatch);

    volatile int system_running = 1;

    SchedulerArgs sched_args = {
        .levels = {&levels[0], &levels[1], &levels[2]},
        .dispatch = &dispatch,
        .system_running = &system_running,
    };

    pthread_t worker_ids[NUM_WORKERS];
    WorkerArgs worker_args[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_args[i].worker_id = i;
        worker_args[i].dispatch = &dispatch;
        worker_args[i].levels[0] = &levels[0];
        worker_args[i].levels[1] = &levels[1];
        worker_args[i].levels[2] = &levels[2];
        worker_args[i].stats = stats;
        pthread_create(&worker_ids[i], NULL, worker_run, &worker_args[i]);
    }

    pthread_t scheduler_thread;
    pthread_create(&scheduler_thread, NULL, scheduler_run, &sched_args);

    WorkloadGenArgs gen_args = {
        .incoming = &levels[0], /* new tasks enter at the highest priority */
        .num_tasks = num_tasks,
        .max_spawn_gap_ms = 40,
    };
    pthread_t gen_thread;
    pthread_create(&gen_thread, NULL, workload_gen_run, &gen_args);
    pthread_join(gen_thread, NULL);

    /* Wait until every generated task has actually completed. */
    for (;;) {
        pthread_mutex_lock(&stats->lock);
        int done = stats->completed_count;
        pthread_mutex_unlock(&stats->lock);
        if (done >= num_tasks) break;
        usleep(20000);
    }

    system_running = 0; /* tells the scheduler to stop and shut down dispatch */
    pthread_join(scheduler_thread, NULL);
    for (int i = 0; i < NUM_WORKERS; i++) pthread_join(worker_ids[i], NULL);

    int wstatus;
    waitpid(monitor_pid, &wstatus, 0); /* reap the monitor child process */

    printf("\n===== final report =====\n");
    printf("tasks completed        : %d\n", stats->completed_count);
    printf("avg waiting time (ms)  : %ld\n",
           stats->completed_count ? stats->total_waiting_ms / stats->completed_count : 0);
    printf("avg turnaround (ms)    : %ld\n",
           stats->completed_count ? stats->total_turnaround_ms / stats->completed_count : 0);
    printf("unsafe requests blocked: %d  (Banker's algorithm avoided a potential deadlock)\n",
           stats->denied_unsafe_requests);

    for (int i = 0; i < MLFQ_LEVELS; i++) task_queue_destroy(&levels[i]);
    task_queue_destroy(&dispatch);
    monitor_shared_destroy(stats, SHM_NAME, SEM_NAME);
    return 0;
}
