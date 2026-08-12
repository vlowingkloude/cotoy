#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "cotoy.h"

void task(void *data, struct coroutine_t *self) {
    int c = *((int *)data);

    for (int i = 0; i < c; i++) {
        printf("thread %ld processing coroutine %d: step %d\n", pthread_self(), c, i+1);
        sleep(1);
        coroutine_yield(self);
    }
}

int main() {
    const int n_threads = 3;

    static int arr[] = {1, 3, 9, 11};
    const int n_tasks = sizeof(arr) / sizeof(arr[0]);

    struct schedule_t *s = coroutine_init(n_threads);

    for (int i = 0; i < n_tasks; i++) {
        co_construct(s, task, arr + i);
    }

    pthread_t workers[n_threads];
    for (int i = 0; i < n_threads; i++) {
        pthread_create(&workers[n_threads], NULL, worker, s);
    }

    bool expected = false;
    bool desired = true;

    while (true) {
        int sum = 0;
        for (int i = 0; i < n_threads; i++) {
            sum += atomic_load_explicit(&(s->thread_status[i]), memory_order_acquire);
        }
        if (sum < n_threads) {
            sleep(1);
        } else {
            break;
        }
    }
    atomic_store(&(s->shutdown), true);
    while (atomic_load_explicit(&(s->n_finished), memory_order_acquire) < n_threads) {
        sleep(1);
    }
    free(s);
    return 0;
}