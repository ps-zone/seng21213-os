#include "scheduler.h"

static pcb_t *ready_head = NULL;
static pcb_t *ready_tail = NULL;
static pcb_t *current_process = NULL;
static uint32_t scheduler_running = 0;

void scheduler_init(void) {
    ready_head = NULL;
    ready_tail = NULL;
    current_process = NULL;
    scheduler_running = 0;
}

void scheduler_add_process(pcb_t *process) {
    if (process == NULL) {
        return;
    }

    process->state = READY;
    process->next = NULL;

    if (ready_head == NULL) {
        ready_head = process;
        ready_tail = process;
    } else {
        ready_tail->next = process;
        ready_tail = process;
    }
}

pcb_t *scheduler_next(void) {
    if (ready_head == NULL) {
        return NULL;
    }

    if (current_process != NULL &&
        current_process->state == RUNNING) {

        current_process->state = READY;
        scheduler_add_process(current_process);
    }

    current_process = ready_head;
    ready_head = ready_head->next;

    if (ready_head == NULL) {
        ready_tail = NULL;
    }

    current_process->next = NULL;
    current_process->state = RUNNING;

    return current_process;
}

pcb_t *scheduler_current(void) {
    return current_process;
}

void scheduler_tick(void) {
    scheduler_next();
}

uint32_t scheduler_switch(uint32_t current_esp) {

    /*
     * Save the stack pointer of the process
     * that was interrupted.
     */
    if (current_process != NULL) {
        current_process->esp = current_esp;
    }

    /*
     * Select the next READY process.
     */
    pcb_t *next = scheduler_next();

    /*
     * If there is no process available,
     * continue using the current stack.
     */
    if (next == NULL) {
        return current_esp;
    }

    /*
     * Assembly will load this ESP and
     * restore the next process.
     */
    return next->esp;
}

void scheduler_start(void) {
    scheduler_running = 1;
}

uint32_t scheduler_is_running(void) {
    return scheduler_running;
}
