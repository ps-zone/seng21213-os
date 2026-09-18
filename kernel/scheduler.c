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
    if (process == NULL || process->state == TERMINATED) {
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
    if (current_process != NULL && current_process->state == RUNNING) {
        current_process->state = READY;
        scheduler_add_process(current_process);
    }

    while (ready_head != NULL && ready_head->state == TERMINATED) {
        pcb_t *terminated = ready_head;
        ready_head = ready_head->next;
        terminated->next = NULL;
    }

    if (ready_head == NULL) {
        ready_tail = NULL;
        current_process = NULL;
        return NULL;
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
    if (current_process != NULL && current_process->state != TERMINATED) {
        current_process->esp = current_esp;
    }

    pcb_t *next = scheduler_next();
    if (next == NULL) {
        return current_esp;
    }

    return next->esp;
}

void scheduler_start(void) {
    scheduler_running = 1;
}

uint32_t scheduler_is_running(void) {
    return scheduler_running;
}
