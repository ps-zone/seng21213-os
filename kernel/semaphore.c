#include "semaphore.h"
#include "scheduler.h"

void sem_init(semaphore_t *sem, int32_t initial_count) {
    sem->count = initial_count;
    sem->wait_head = 0;
    sem->wait_tail = 0;
    sem->wait_count = 0;

    for (uint32_t i = 0; i < SEMAPHORE_MAX_WAITERS; i++) {
        sem->waiters[i] = NULL;
    }
}

void sem_wait(semaphore_t *sem) {
    pcb_t *current = scheduler_current();

    if (current == NULL) {
        return;
    }

    __asm__ __volatile__("cli");

    /*
     * Resource is available.
     * Take one unit and continue.
     */
    if (sem->count > 0) {
        sem->count--;
        __asm__ __volatile__("sti");
        return;
    }

    /*
     * No resource is available.
     * Put the current thread into the waiting queue.
     */
    if (sem->wait_count < SEMAPHORE_MAX_WAITERS) {
        sem->waiters[sem->wait_tail] = current;

        sem->wait_tail =
            (sem->wait_tail + 1) % SEMAPHORE_MAX_WAITERS;

        sem->wait_count++;

        current->state = BLOCKED;
    }

    __asm__ __volatile__("sti");

    /*
     * Sleep while this thread is blocked.
     * Timer interrupts allow another thread to run.
     */
    while (current->state == BLOCKED) {
        __asm__ __volatile__("hlt");
    }
}

void sem_signal(semaphore_t *sem) {
    __asm__ __volatile__("cli");

    /*
     * If another thread is waiting, wake that thread.
     * The released resource is transferred directly to it.
     */
    if (sem->wait_count > 0) {
        pcb_t *next = sem->waiters[sem->wait_head];

        sem->waiters[sem->wait_head] = NULL;

        sem->wait_head =
            (sem->wait_head + 1) % SEMAPHORE_MAX_WAITERS;

        sem->wait_count--;

        if (next != NULL) {
            next->state = READY;
            scheduler_add_process(next);
        }
    } else {
        /*
         * Nobody is waiting, so increase the available count.
         */
        sem->count++;
    }

    __asm__ __volatile__("sti");
}
