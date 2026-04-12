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

static void *worker(void *arg) {
    WorkerArgs *a = (WorkerArgs *)arg;

    int width = LB_width(a->state);

    for (int step = 0; step < a->steps; step++) {

        // Compute assigned rows
        for (int y = a->start_row; y < a->end_row; y++) {
            for (int x = 1; x < width - 1; x++) {

                int neighbors =
                    LB_get(a->state, x-1, y-1) +
                    LB_get(a->state, x,   y-1) +
                    LB_get(a->state, x+1, y-1) +
                    LB_get(a->state, x-1, y)   +
                    LB_get(a->state, x+1, y)   +
                    LB_get(a->state, x-1, y+1) +
                    LB_get(a->state, x,   y+1) +
                    LB_get(a->state, x+1, y+1);

                int current = LB_get(a->state, x, y);
                int next = 0;

                if (neighbors == 3 || (neighbors == 2 && current)) {
                    next = 1;
                }

                LB_set(a->next_state, x, y, next);
            }
        }

        // Wait for all threads to finish computing
        pthread_barrier_wait(a->barrier);

        // One thread swaps boards
        if (a->thread_id == 0) {
            LB_swap(a->state, a->next_state);
        }

        // Wait until swap is done
        pthread_barrier_wait(a->barrier);
    }

    return NULL;
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    if (threads <= 0 || steps <= 0) return;

    int width = LB_width(state);
    int height = LB_height(state);

    LifeBoard *next_state = LB_new(width, height);
    if (!next_state) {
        fprintf(stderr, "Failed to allocate next_state\n");
        exit(1);
    }

    pthread_t *tids = malloc(sizeof(pthread_t) * threads);
    WorkerArgs *args = malloc(sizeof(WorkerArgs) * threads);

    if (!tids || !args) {
        fprintf(stderr, "Memory allocation failed\n");
        free(tids);
        free(args);
        LB_del(next_state);
        exit(1);
    }

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads);

    // ---- ROW SPLITTING ----
    int interior_rows = height - 2;
    if (interior_rows < 0) interior_rows = 0;

    int base = interior_rows / threads;
    int extra = interior_rows % threads;

    int current = 1;

    for (int i = 0; i < threads; i++) {
        int rows = base + (i < extra ? 1 : 0);

        args[i].thread_id = i;
        args[i].threads = threads;
        args[i].steps = steps;

        args[i].start_row = current;
        args[i].end_row = current + rows;

        args[i].state = state;
        args[i].next_state = next_state;
        args[i].barrier = &barrier;

        current = args[i].end_row;
    }

    // ---- CREATE THREADS ----
    for (int i = 0; i < threads; i++) {
        pthread_create(&tids[i], NULL, worker, &args[i]);
    }

    // ---- JOIN THREADS ----
    for (int i = 0; i < threads; i++) {
        pthread_join(tids[i], NULL);
    }

    // ---- CLEANUP ----
    pthread_barrier_destroy(&barrier);
    LB_del(next_state);
    free(tids);
    free(args);
}
