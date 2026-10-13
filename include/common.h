#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <pthread.h>

/* ---- Resource model ---------------------------------------------------
 * Three resource TYPES, each with a fixed number of instances.
 * This is exactly the shape the Banker's Algorithm expects:
 * Available[NUM_RESOURCES], and per-task Allocation[] / Max[] / Need[].
 * ------------------------------------------------------------------- */
#define NUM_RESOURCES     3
#define RES_CPU_CORES     0
#define RES_IO_CHANNELS   1
#define RES_MEM_BLOCKS    2

#define TOTAL_CPU_CORES   4
#define TOTAL_IO_CHANNELS 2
#define TOTAL_MEM_BLOCKS  8

#define MAX_TASKS         1024 /* upper bound used to size Banker's arrays */
#define MLFQ_LEVELS       3

typedef enum {
    TASK_CPU_BOUND,
    TASK_IO_BOUND,
    TASK_MIXED
} TaskType;

typedef struct Task {
    int id;
    TaskType type;

    int burst_ms;             /* remaining CPU burst, decremented per quantum */
    int total_burst_ms;       /* original burst, for stats */
    int queue_level;          /* current MLFQ level: 0 = highest priority */

    int max_need[NUM_RESOURCES];   /* max resources this task will ever hold */
    int need1[NUM_RESOURCES];      /* initial claim requested at start */
    int allocated[NUM_RESOURCES];  /* currently allocated (0 until granted) */
    int phase2_done;                /* have we asked for the max_need-need1 top-up yet? */

    struct timespec arrival_time;
    struct timespec start_time;      /* first time it got the CPU */
    struct timespec completion_time;
    int started;
    int registered;            /* has resource_mgr_register() been called yet? */

    long waiting_time_ms;

    struct Task *next;         /* intrusive singly-linked list for the queue */
} Task;

/* ---- Shared statistics (placed in shared memory for the monitor) ----- */
typedef struct {
    int completed_count;
    long total_waiting_ms;
    long total_turnaround_ms;
    int active_tasks;
    int denied_unsafe_requests;   /* times Banker's algorithm blocked a grant */
    pthread_mutex_t lock;         /* process-shared mutex, see monitor_ipc.c */
} SharedStats;

#endif /* COMMON_H */
