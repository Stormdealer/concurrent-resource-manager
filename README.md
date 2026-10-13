# Concurrent Resource Manager for Heterogeneous Workloads

A simulated OS resource manager that schedules heterogeneous tasks
(CPU-bound / I/O-bound / mixed) across a pool of worker threads, using
a Multilevel Feedback Queue for CPU scheduling and the Banker's
Algorithm for deadlock-avoiding resource allocation.

**Course:** Operating Systems Lab project
**Stack:** C (gnu11), GCC, POSIX threads, POSIX shared memory and semaphores, Linux/Ubuntu (WSL2), Git/GitHub

---

## Project status

### Done so far

| Area | Status | Details |
|---|---|---|
| Architecture and algorithm design | Done | Module breakdown, task-lifecycle flowchart, MLFQ and Banker's design |
| Thread-safe task queue | Done | Mutex plus condition variable (producer-consumer), clean shutdown |
| Workload generator | Done | CPU-bound, I/O-bound and mixed tasks with random bursts and resource claims |
| MLFQ scheduler | Done | 3 levels with 20/50/100 ms quanta; tasks that use a full quantum are demoted |
| Banker's Algorithm | Done | Safety check before every grant; two-phase requests so hold-and-wait really happens |
| Worker thread pool | Done | 3 pthreads; non-blocking resource requests; denied tasks requeue instead of blocking a worker |
| IPC monitor | Done | `fork()`ed monitor process reads live stats from POSIX shared memory (process-shared mutex plus named semaphore) |
| Statistics | Done | Average waiting time, average turnaround time, unsafe requests blocked |
| Build system | Done | Makefile; compiles with `-Wall -Wextra` and zero warnings |
| Testing | Partly done | Runs from 1 to 200 tasks complete without hanging (see `tests/test_cases.md`) |
| Running on Linux | Done | Built and run on Ubuntu under WSL2 (12 tasks: 12/12 completed) |

### Bugs found and fixed (useful for the report and viva)

1. **Premature Banker's registration.** Tasks were registered with the
   resource manager at creation, so the safety check had to prove all
   pending tasks could finish at once, which is impossible. Fixed by
   registering a task only when a worker first picks it up.
2. **Worker blocked on its own resource request.** A blocked worker
   could not run the tasks that held the resources it was waiting for,
   a circular wait. Fixed with a non-blocking `resource_mgr_try_request`
   that requeues the task on denial.
3. **Banker's check was never exercised.** Requesting the full claim
   in one shot meant hold-and-wait never occurred. Fixed with two-phase
   acquisition (`need1`, then a top-up at half burst).
4. **MLFQ starvation at high load (about 120+ tasks).** Denied tasks
   were requeued at level 0, and under strict priority they starved the
   demoted tasks that held the resources. Run time grew badly and the
   program looked stuck. Fixed by requeueing denied tasks at the lowest
   level. 120 tasks now finish in about 16 s.
5. **Silent 64-task limit.** `MAX_TASKS` was 64, so extra tasks were
   not tracked by the safety check. Raised to 1024, and `main` now
   rejects values outside 1 to 1024.
6. **Duplicated or garbled output when piped.** `fork()` copied
   unflushed stdout into the monitor process. Fixed with line-buffered
   stdout, so `./crm 25 | tee out.txt` is safe.

### Remaining work

| Item | Why it matters | Notes |
|---|---|---|
| Comparative performance analysis | Needed for the performance-evaluation part of Review II | Run the same fixed-seed workload under FCFS or Round Robin and MLFQ; tabulate waiting and turnaround time |
| Priority boost for MLFQ | Standard MLFQ anti-starvation rule that the design mentions | Periodically move all tasks to level 0; currently starvation is avoided only by the requeue-at-lowest-level fix |
| Real memory management | Memory is currently only a count of opaque tokens | Add a first-fit or best-fit block allocator with fragmentation stats |
| CPU utilization metric | Mentioned in the design, not yet computed | Track busy time per worker versus wall-clock time |
| Test documentation update | `tests/test_cases.md` predates the starvation fix | Add the 120-task case, the invalid-argument case (now handled), and the before/after comparison |
| Input validation and config | Only the task-count argument is validated | Optional: command-line flags for worker count, resource totals, and scheduler choice |
| Project proposal document | Required deliverable | Problem, objectives, approach, timeline |
| Review I presentation | Required deliverable | Architecture diagram, working demo, remaining plan |
| Final report and Review II | Required deliverable | Design, implementation, test results, comparison, conclusions |
| GitHub repository | Git/GitHub is part of the topic | `git init`, commit, push; `.gitignore` is already included |

---

## Build and run

```bash
make            # builds ./crm
make run        # builds and runs with 12 tasks
./crm 25        # run directly with a custom task count (1 to 1024)
make clean      # remove build artifacts
```

Requires `gcc` and POSIX threads and shared memory (`-pthread -lrt`).
It needs real Linux: use Ubuntu, or WSL2 on Windows. Git Bash will not
work because Windows has no `fork()` or `shm_open`.

Run time grows roughly linearly with task count because only one or
two tasks fit in the resource pool at once (12 tasks take about 2 s,
120 tasks about 16 s). For demos, 12 to 40 tasks is plenty.

## What each module demonstrates

| Module | OS concept |
|---|---|
| `src/task_queue.*` | Mutexes, condition variables, producer-consumer |
| `src/workload_gen.*` | Task/process modeling, dynamic arrivals |
| `src/scheduler.*` | CPU scheduling: Multilevel Feedback Queue (MLFQ) |
| `src/resource_mgr.*` | Resource allocation, deadlock avoidance (Banker's Algorithm) |
| `src/worker.*` | Multithreading, POSIX pthreads, worker-pool execution |
| `src/monitor_ipc.*` | IPC: `fork()`, POSIX shared memory, named semaphore |
| `src/stats.*` | Performance metrics: waiting time, turnaround time, blocked requests |

## How resource acquisition works

Each task has a `max_need[]` (its full resource claim across CPU
cores, I/O channels, and memory blocks) and requests it in **two
phases**: an initial partial claim (`need1`) at first dispatch, and a
top-up to the full claim partway through its burst. This deliberately
creates hold-and-wait, where a task holds some resources while
waiting for more, so the Banker's Algorithm's safety check is
actually exercised and not just a resource-availability check. Every
request runs a full safety simulation across all currently active
tasks before committing; an unsafe grant is rejected and the task
retries later instead of blocking its worker thread. A task that is
denied its first claim is requeued at the lowest MLFQ level so it
cannot starve the tasks that already hold resources.

## Sample output

Real output from `./crm 12` on Ubuntu (WSL2), tail only:

```
[monitor] completed=12/12  avg_wait=349ms  avg_turnaround=581ms  unsafe_requests_blocked=1

===== final report =====
tasks completed        : 12
avg waiting time (ms)  : 349
avg turnaround (ms)    : 581
unsafe requests blocked: 1  (Banker's algorithm avoided a potential deadlock)
```

Exact numbers vary slightly between runs because thread scheduling is
nondeterministic, even with a fixed random seed.

## Design notes

- Resource pool sizes are fixed in `include/common.h`
  (`TOTAL_CPU_CORES`, `TOTAL_IO_CHANNELS`, `TOTAL_MEM_BLOCKS`); lower
  them to see more Banker's-algorithm denials under contention.
- The random seed is fixed (`srand(42)` in `main.c`) so task sets are
  reproducible for grading and demos; remove it for true randomness.
- Only the monitor runs as a separate process; the scheduler, generator
  and workers are threads in the main process.
