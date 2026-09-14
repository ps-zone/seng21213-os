#ifndef MUTEX_H
#define MUTEX_H

#include "../include/types.h"
#include "process.h"

#define MUTEX_MAX_WAITERS 16

typedef struct {
    uint32_t locked;
    pcb_t *owner;

    pcb_t *waiters[MUTEX_MAX_WAITERS];
    uint32_t wait_head;
    uint32_t wait_tail;
    uint32_t wait_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
