#include "scheduler.h"

static pcb_t *ready_head = NULL;
static pcb_t *ready_tail = NULL;
static pcb_t *current_process = NULL;

void scheduler_init(void) {
    ready_head = NULL;
    ready_tail = NULL;
    current_process = NULL;
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
