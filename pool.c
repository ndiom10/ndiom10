#define _XOPEN_SOURCE 700

#include "pool.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define MAX_TASKS 200

struct PoolTask {
    int id;
    task_fn fn;
    void *arg;
    void *result;
    bool exists;
    bool finished;
};

static pthread_t *workers = NULL;
static int worker_count = 0;

static struct PoolTask tasks[MAX_TASKS];
static int queue[MAX_TASKS];
static int q_head = 0;
static int q_tail = 0;
static int q_size = 0;

static int next_task_id = 0;
static int active_tasks = 0;
static bool stopping = false;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t done_cv = PTHREAD_COND_INITIALIZER;

static void reset_state(void) {
    for (int i = 0; i < MAX_TASKS; i += 1) {
        tasks[i].id = i;
        tasks[i].fn = NULL;
        tasks[i].arg = NULL;
        tasks[i].result = NULL;
        tasks[i].exists = false;
        tasks[i].finished = false;
    }

    q_head = 0;
    q_tail = 0;
    q_size = 0;
    next_task_id = 0;
    active_tasks = 0;
    stopping = false;
}

static bool no_pending_work(void) {
    return q_size == 0 && active_tasks == 0;
}

static void *worker_main(void *unused) {
    (void)unused;

    while (1) {
        pthread_mutex_lock(&lock);

        while (q_size == 0 && !stopping) {
            pthread_cond_wait(&work_cv, &lock);
        }

        if (q_size == 0 && stopping) {
            pthread_mutex_unlock(&lock);
            break;
        }

        int task_id = queue[q_head];
        q_head = (q_head + 1) % MAX_TASKS;
        q_size -= 1;
        active_tasks += 1;

        task_fn fn = tasks[task_id].fn;
        void *arg = tasks[task_id].arg;

        pthread_mutex_unlock(&lock);

        void *result = fn(arg);

        pthread_mutex_lock(&lock);

        tasks[task_id].result = result;
        tasks[task_id].finished = true;
        active_tasks -= 1;

        if (no_pending_work()) {
            pthread_cond_broadcast(&done_cv);
        }

        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

void pool_setup(int threads) {
    pthread_mutex_lock(&lock);

    reset_state();
    worker_count = threads;
    workers = malloc(sizeof(pthread_t) * (size_t)worker_count);

    pthread_mutex_unlock(&lock);

    for (int i = 0; i < worker_count; i += 1) {
        pthread_create(&workers[i], NULL, worker_main, NULL);
    }
}

int pool_submit_task(task_fn task, void *argument) {
    pthread_mutex_lock(&lock);

    if (next_task_id >= MAX_TASKS) {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    int id = next_task_id;
    next_task_id += 1;

    tasks[id].id = id;
    tasks[id].fn = task;
    tasks[id].arg = argument;
    tasks[id].result = NULL;
    tasks[id].exists = true;
    tasks[id].finished = false;

    queue[q_tail] = id;
    q_tail = (q_tail + 1) % MAX_TASKS;
    q_size += 1;

    pthread_cond_signal(&work_cv);
    pthread_mutex_unlock(&lock);

    return id;
}

void *pool_get_task_result(int task_id) {
    pthread_mutex_lock(&lock);

    if (task_id < 0 || task_id >= MAX_TASKS || !tasks[task_id].exists || !tasks[task_id].finished) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    void *result = tasks[task_id].result;
    pthread_mutex_unlock(&lock);
    return result;
}

void pool_wait(void) {
    pthread_mutex_lock(&lock);

    int target = next_task_id;

    while (1) {
        bool all_done = true;
        for (int i = 0; i < target; i += 1) {
            if (tasks[i].exists && !tasks[i].finished) {
                all_done = false;
                break;
            }
        }

        if (all_done) {
            pthread_mutex_unlock(&lock);
            return;
        }

        pthread_cond_wait(&done_cv, &lock);
    }
}

void pool_stop(void) {
    pthread_mutex_lock(&lock);

    stopping = true;
    pthread_cond_broadcast(&work_cv);

    while (!no_pending_work()) {
        pthread_cond_wait(&done_cv, &lock);
    }

    pthread_mutex_unlock(&lock);

    for (int i = 0; i < worker_count; i += 1) {
        pthread_join(workers[i], NULL);
    }

    free(workers);
    workers = NULL;
    worker_count = 0;
}
