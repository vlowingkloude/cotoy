#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>

#include "cotoy.h"

void co_wrapper(void *data, struct coroutine_t *co) {
    co->func(data, co);
    // need to set the status after the func is really done
    co->status = COROUTINE_END;
}

void co_construct(struct schedule_t *s, coroutine_function_t func, void *data) {
    struct coroutine_t *co = malloc(sizeof(struct coroutine_t));
    co->func = func;
    co->data = data;
    co->schedule = s;
    co->status = COROUTINE_READY;
    co->stack = malloc(STACK_SIZE);

    co->self = co;

    getcontext(&co->ctx);
    co->ctx.uc_stack.ss_sp = co->stack;
    co->ctx.uc_stack.ss_size = STACK_SIZE;
    co->ctx.uc_link = malloc(sizeof(ucontext_t)); // for context return

    makecontext(&co->ctx, (void (*)(void))co_wrapper, 2, data, co);
    queue_push(s, co);
}

void co_deconstruct(struct coroutine_t *co) {
    free(co->ctx.uc_link);
    free(co->stack);
    free(co);
}

void queue_push(struct schedule_t *s, struct coroutine_t *co) {
    while (true) {
        bool expected = false;
        bool desired = true;
        if (atomic_compare_exchange_weak_explicit(&(s->lock), &expected, desired, memory_order_acq_rel, memory_order_acquire)) {
            if (s->count >= MAX_N_COROUTINE) {
                // not very correct here, we just assume some threads are still processing
                atomic_store_explicit(&(s->lock), false, memory_order_release);
                continue;
            }
            break;
        }
    }
    s->queue[s->tail] = co;
    s->tail = (s->tail + 1) % MAX_N_COROUTINE;
    s->count++;
    atomic_store_explicit(&(s->lock), false, memory_order_release);
}

struct coroutine_t *queue_pop(struct schedule_t *s) {
    while (true) {
        bool expected = false;
        bool desired = true;
        if (atomic_compare_exchange_weak_explicit(&(s->lock), &expected, desired, memory_order_acq_rel, memory_order_acquire)) {
            break;
        }
    }
    if (s->count == 0) {
        atomic_store_explicit(&(s->lock), false, memory_order_release);
        return NULL;
    }

    struct coroutine_t *co = s->queue[s->head];
    s->head = (s->head + 1) % MAX_N_COROUTINE;
    s->count--;
    atomic_store_explicit(&(s->lock), false, memory_order_release);
    return co;
}

struct schedule_t *coroutine_init(const size_t n_threads) {
    struct schedule_t *schedule = malloc(sizeof(struct schedule_t));

    schedule->head = schedule->tail = schedule->count = 0;
    atomic_store_explicit(&(schedule->shutdown), false, memory_order_release);
    atomic_store_explicit(&(schedule->lock), false, memory_order_release);
    atomic_store_explicit(&(schedule->n_finished), 0, memory_order_release);

    schedule->n_threads = n_threads;
    atomic_store_explicit(&(schedule->thread_shared_int), n_threads - 1, memory_order_release);
    schedule->thread_status = malloc(sizeof(atomic_bool) * n_threads);
    for (size_t i = 0; i < n_threads; i++) {
        atomic_store_explicit(schedule->thread_status + i, false, memory_order_release);
    }

    return schedule;
}

void coroutine_yield(struct coroutine_t *co) {
    co->status = COROUTINE_SUSPEND;
    swapcontext(&(co->ctx), co->ctx.uc_link);
}

// we so far don't really return values
void *worker(void *arg) {
    struct schedule_t *s = (struct schedule_t *)arg;
    const int tid = atomic_fetch_sub_explicit(&(s->thread_shared_int), 1, memory_order_acq_rel);

    while (true) {
        if (atomic_load_explicit(&(s->shutdown), memory_order_acquire)) {
            atomic_fetch_add_explicit(&(s->n_finished), 1, memory_order_acq_rel);
            return NULL;
        }
        struct coroutine_t *co = queue_pop(s);
        if (co == NULL) {
            // so currently no work to do, we simply sleep a while
            atomic_store_explicit(&(s->thread_status[tid]), true, memory_order_release);
            sleep(1);
            continue;
        }
        co->status = COROUTINE_RUNNING;
        swapcontext(co->ctx.uc_link, &(co->ctx));
        if (co->status != COROUTINE_END) {
            queue_push(s, co);
        } else {
            co_deconstruct(co);
        }
    }
}