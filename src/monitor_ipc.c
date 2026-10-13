#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>
#include "monitor_ipc.h"

SharedStats *monitor_shared_create(const char *shm_name) {
    shm_unlink(shm_name); /* clean up any stale segment from a previous run */

    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0644);
    if (fd < 0) { perror("shm_open"); exit(1); }
    if (ftruncate(fd, sizeof(SharedStats)) != 0) { perror("ftruncate"); exit(1); }

    SharedStats *stats = mmap(NULL, sizeof(SharedStats),
                               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (stats == MAP_FAILED) { perror("mmap"); exit(1); }

    memset(stats, 0, sizeof(SharedStats));

    /* The mutex itself lives INSIDE shared memory, so it must be created
     * with the PROCESS_SHARED attribute to be usable across fork(). */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&stats->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    return stats;
}

pid_t monitor_spawn(SharedStats *stats, const char *sem_name,
                     int poll_interval_ms, int expected_tasks) {
    sem_unlink(sem_name);
    sem_t *ready = sem_open(sem_name, O_CREAT, 0644, 0);
    if (ready == SEM_FAILED) { perror("sem_open"); exit(1); }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }

    if (pid == 0) {
        /* --- child: the monitor process --- */
        sem_wait(ready); /* wait until parent signals shared memory is ready */

        for (;;) {
            pthread_mutex_lock(&stats->lock);
            int completed = stats->completed_count;
            long avg_wait = completed ? stats->total_waiting_ms / completed : 0;
            long avg_turn = completed ? stats->total_turnaround_ms / completed : 0;
            int denied = stats->denied_unsafe_requests;
            pthread_mutex_unlock(&stats->lock);

            printf("[monitor] completed=%d/%d  avg_wait=%ldms  "
                   "avg_turnaround=%ldms  unsafe_requests_blocked=%d\n",
                   completed, expected_tasks, avg_wait, avg_turn, denied);

            if (completed >= expected_tasks) break;
            usleep(poll_interval_ms * 1000);
        }
        sem_close(ready);
        _exit(0);
    }

    /* --- parent --- */
    sem_post(ready); /* shared memory is fully initialized; let the child read it */
    sem_close(ready);
    return pid;
}

void monitor_shared_destroy(SharedStats *stats, const char *shm_name,
                             const char *sem_name) {
    munmap(stats, sizeof(SharedStats));
    shm_unlink(shm_name);
    sem_unlink(sem_name);
}
