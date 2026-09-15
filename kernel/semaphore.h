#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "../include/types.h"
#include "process.h"

#define SEMAPHORE_MAX_WAITERS 16

typedef struct {
    int32_t count;

    pcb_t *waiters[SEMAPHORE_MAX_WAITERS];
    uint32_t wait_head;
    uint32_t wait_tail;
    uint32_t wait_count;
} semaphore_t;

/* Initialize a counting semaphore */
void sem_init(semaphore_t *sem, int32_t initial_count);

/* Wait / acquire */
void sem_wait(semaphore_t *sem);

/* Signal / release */
void sem_signal(semaphore_t *sem);

#endif
