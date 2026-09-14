#include "mutex.h"
#include "scheduler.h"

void mutex_init(mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = NULL;

    mutex->wait_head = 0;
    mutex->wait_tail = 0;
    mutex->wait_count = 0;

    for (uint32_t i = 0; i < MUTEX_MAX_WAITERS; i++) {
        mutex->waiters[i] = NULL;
    }
}

void mutex_lock(mutex_t *mutex) {
    pcb_t *current = scheduler_current();

    if (current == NULL) {
        return;
    }

    for (;;) {

        __asm__ __volatile__("cli");

        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner = current;

            __asm__ __volatile__("sti");
            return;
        }

        /*
         * Already owned by this execution context.
         * Avoid inserting it into the wait queue twice.
         */
        if (mutex->owner == current) {
            __asm__ __volatile__("sti");
            return;
        }

        /*
         * Put current execution context into the wait queue.
         */
        if (mutex->wait_count < MUTEX_MAX_WAITERS) {

            mutex->waiters[mutex->wait_tail] = current;

            mutex->wait_tail =
                (mutex->wait_tail + 1) % MUTEX_MAX_WAITERS;

            mutex->wait_count++;

            current->state = BLOCKED;
        }

        __asm__ __volatile__("sti");

        /*
         * Wait until another execution context unlocks the mutex.
         * Timer IRQ will switch us away because our state is BLOCKED.
         */
        while (current->state == BLOCKED) {
            __asm__ __volatile__("hlt");
        }
    }
}

void mutex_unlock(mutex_t *mutex) {
    pcb_t *current = scheduler_current();

    __asm__ __volatile__("cli");

    /*
     * Only the owner may unlock.
     */
    if (!mutex->locked || mutex->owner != current) {
        __asm__ __volatile__("sti");
        return;
    }

    mutex->locked = 0;
    mutex->owner = NULL;

    /*
     * Wake one blocked waiter.
     */
    if (mutex->wait_count > 0) {

        pcb_t *next =
            mutex->waiters[mutex->wait_head];

        mutex->waiters[mutex->wait_head] = NULL;

        mutex->wait_head =
            (mutex->wait_head + 1) % MUTEX_MAX_WAITERS;

        mutex->wait_count--;

        if (next != NULL) {
            next->state = READY;
            scheduler_add_process(next);
        }
    }

    __asm__ __volatile__("sti");
}
