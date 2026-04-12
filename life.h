#include "life.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int thread_id;
    int threads;
    int steps;

    int start_row;   // inclusive
    int end_row;     // exclusive

    LifeBoard *state;
    LifeBoard *next_state;

    pthread_barrier_t *barrier;
} WorkerArgs;

static void *worker_run(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;

    for (int step = 0; step < args->steps; step++) {
        /* Compute assigned rows for next generation */
        for (int y = args->start_row; y < args->end_row; y++) {
            for (int x = 1; x < LB_width(args->state) - 1; x++) {
                int neighbors = 0;

                neighbors += LB_get(args->state, x - 1, y - 1);
                neighbors += LB_get(args->state, x,     y - 1);
                neighbors += LB_get(args->state, x + 1, y - 1);
                neighbors += LB_get(args->state, x - 1, y);
                neighbors += LB_get(args->state, x + 1, y);
                neighbors += LB_get(args->state, x - 1, y + 1);
                neighbors += LB_get(args->state, x,     y + 1);
                neighbors += LB_get(args->state, x + 1, y + 1);

                int current = LB_get(args->state, x, y);
                int next = 0;

                if (neighbors == 3 || (neighbors == 2 && current)) {
                    next = 1;
                }

                LB_set(args->next_state, x, y, next);
            }
        }

        /* Wait until all threads finish computing this generation */
        pthread_barrier_wait(args->barrier);

        /* One thread performs the swap */
        if (args->thread_id == 0) {
            LB_swap(args->state, args->next_state);
        }

        /* Wait until swap is complete before next iteration */
        pthread_barrier_wait(args->barrier);
    }

    return NULL;
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    if (steps <= 0 || threads <= 0) {
        return;
    }

    int width = LB_width(state);
    int height = LB_height(state);

    LifeBoard *next_state = LB_new(width, height);
    if (next_state == NULL) {
        fprintf(stderr, "Failed to allocate next_state\n");
        exit(1);
    }

    pthread_t *tids = malloc(sizeof(pthread_t) * threads);
    WorkerArgs *args = malloc(sizeof(WorkerArgs) * threads);
    if (tids == NULL || args == NULL) {
        fprintf(stderr, "Failed to allocate thread structures\n");
        LB_del(next_state);
        free(tids);
        free(args);
        exit(1);
    }

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads);

    /*
     * Split only the interior rows [1, height-1) because borders are not updated.
     * Interior row count = height - 2.
     */
    int interior_rows = height - 2;
    if (interior_rows < 0) {
        interior_rows = 0;
    }

    int base = interior_rows / threads;
    int extra = interior_rows % threads;

    int next_row = 1;
    for (int i = 0; i < threads; i++) {
        int rows_for_this_thread = base + (i < extra ? 1 : 0);

        args[i].thread_id = i;
        args[i].threads = threads;
        args[i].steps = steps;
        args[i].start_row = next_row;
        args[i].end_row = next_row + rows_for_this_thread;
        args[i].state = state;
        args[i].next_state = next_state;
        args[i].barrier = &barrier;

        next_row = args[i].end_row;
    }

    for (int i = 0; i < threads; i++) {
        pthread_create(&tids[i], NULL, worker_run, &args[i]);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tids[i], NULL);
    }

    pthread_barrier_destroy(&barrier);
    LB_del(next_state);
    free(tids);
    free(args);
}
