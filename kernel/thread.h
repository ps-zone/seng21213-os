#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"

#define MAX_THREADS 16
#define THREAD_STACK_SIZE 4096

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t tid;

    thread_state_t state;

    uint32_t esp;
    uint32_t eip;

    void (*entry)(void *);
    void *arg;

    uint32_t stack[THREAD_STACK_SIZE / 4];

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
