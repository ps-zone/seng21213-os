#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"
#include "process.h"

#define MAX_THREADS 16

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t tid;

    thread_state_t state;

    void (*entry)(void *);
    void *arg;

    /*
     * CPU execution context.
     * We reuse the Stage 1 PCB format so the existing
     * round-robin scheduler can schedule this thread.
     */
    pcb_t context;

    struct thread *next;

} thread_t;

void thread_init(void);

thread_t *thread_create(
    void (*entry)(void *),
    void *arg
);

thread_t *thread_get(uint32_t index);

uint32_t thread_count(void);

const char *thread_state_name(thread_state_t state);

#endif
