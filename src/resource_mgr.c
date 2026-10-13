#include <string.h>
#include <pthread.h>
#include "resource_mgr.h"

/* ---- module-private state ---------------------------------------- */
static int available[NUM_RESOURCES];
static Task *active_tasks[MAX_TASKS];
static int active_count = 0;
static int denied_count = 0;

static pthread_mutex_t mgr_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  mgr_cond = PTHREAD_COND_INITIALIZER;

void resource_mgr_init(void) {
    available[RES_CPU_CORES]   = TOTAL_CPU_CORES;
    available[RES_IO_CHANNELS] = TOTAL_IO_CHANNELS;
    available[RES_MEM_BLOCKS]  = TOTAL_MEM_BLOCKS;
    active_count = 0;
    denied_count = 0;
}

void resource_mgr_register(Task *t) {
    pthread_mutex_lock(&mgr_lock);
    for (int j = 0; j < NUM_RESOURCES; j++) t->allocated[j] = 0;
    if (active_count < MAX_TASKS) active_tasks[active_count++] = t;
    pthread_mutex_unlock(&mgr_lock);
}

void resource_mgr_unregister(Task *t) {
    pthread_mutex_lock(&mgr_lock);
    for (int i = 0; i < active_count; i++) {
        if (active_tasks[i] == t) {
            active_tasks[i] = active_tasks[active_count - 1];
            active_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mgr_lock);
}

/* Classic Banker's safety algorithm. Caller must hold mgr_lock.
 * Returns 1 if the CURRENT allocation state (given `available` as-is)
 * has a safe sequence, 0 otherwise. */
static int is_safe_locked(void) {
    int work[NUM_RESOURCES];
    int finish[MAX_TASKS] = {0};
    memcpy(work, available, sizeof(work));

    int done = 0;
    while (done < active_count) {
        int progressed = 0;
        for (int i = 0; i < active_count; i++) {
            if (finish[i]) continue;
            Task *t = active_tasks[i];
            int can_finish = 1;
            for (int j = 0; j < NUM_RESOURCES; j++) {
                int need_j = t->max_need[j] - t->allocated[j];
                if (need_j > work[j]) { can_finish = 0; break; }
            }
            if (can_finish) {
                for (int j = 0; j < NUM_RESOURCES; j++) work[j] += t->allocated[j];
                finish[i] = 1;
                done++;
                progressed = 1;
            }
        }
        if (!progressed) return 0; /* no task could finish -> unsafe */
    }
    return 1;
}

int resource_mgr_try_request(Task *t, const int *req) {
    pthread_mutex_lock(&mgr_lock);

    int fits = 1;
    for (int j = 0; j < NUM_RESOURCES; j++) {
        if (req[j] > available[j]) { fits = 0; break; }
    }

    int granted = 0;
    if (fits) {
        /* tentatively grant on top of whatever t already holds */
        for (int j = 0; j < NUM_RESOURCES; j++) {
            available[j] -= req[j];
            t->allocated[j] += req[j];
        }
        if (is_safe_locked()) {
            granted = 1; /* commit: resources stay granted */
        } else {
            /* unsafe: roll back and count the denial */
            for (int j = 0; j < NUM_RESOURCES; j++) {
                available[j] += req[j];
                t->allocated[j] -= req[j];
            }
            denied_count++;
        }
    }

    pthread_mutex_unlock(&mgr_lock);
    return granted;
}

void resource_mgr_release(Task *t) {
    pthread_mutex_lock(&mgr_lock);
    for (int j = 0; j < NUM_RESOURCES; j++) {
        available[j] += t->allocated[j];
        t->allocated[j] = 0;
    }
    pthread_cond_broadcast(&mgr_cond); /* wake tasks waiting on resources */
    pthread_mutex_unlock(&mgr_lock);
}

int resource_mgr_denied_count(void) {
    pthread_mutex_lock(&mgr_lock);
    int n = denied_count;
    pthread_mutex_unlock(&mgr_lock);
    return n;
}
