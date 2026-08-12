#ifndef COTOY_COTOY_H
#define COTOY_COTOY_H

#include <stdatomic.h>
#include <ucontext.h>

#define STACK_SIZE (64 * 1024)
#define MAX_N_COROUTINE 100

struct schedule_t;
struct coroutine_t;

typedef void (*coroutine_function_t)(void *, struct coroutine_t *);

enum coroutine_status {
    COROUTINE_READY,
    COROUTINE_RUNNING,
    COROUTINE_SUSPEND,
    COROUTINE_END
};

struct coroutine_t {
    struct coroutine_t *self;
    coroutine_function_t func; // the coroutine function
    void *data; // input to func
    ucontext_t ctx; // own context
    struct schedule_t *schedule; // a toy scheduler
    enum coroutine_status status; // coroutine status
    void *stack; // own stack
};

struct schedule_t {
    struct coroutine_t *queue[MAX_N_COROUTINE]; // process queue
    size_t head, tail, count;
    atomic_bool lock; // lock for above vars. threads will just busy wait
    atomic_bool shutdown;
    size_t n_threads;
    atomic_int thread_shared_int;
    atomic_bool *thread_status;
    atomic_int n_finished;
};

void queue_push(struct schedule_t *s, struct coroutine_t *co);
struct coroutine_t *queue_pop(struct schedule_t *s);

void co_wrapper(void *data, struct coroutine_t *co);
void co_construct(struct schedule_t *s, coroutine_function_t func, void *data);
void co_deconstruct(struct coroutine_t *co);

struct schedule_t *coroutine_init(size_t n_threads);

void coroutine_yield(struct coroutine_t *co);
void *worker(void *arg);

#endif //COTOY_COTOY_H
