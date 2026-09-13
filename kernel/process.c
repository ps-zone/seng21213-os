#include "process.h"

static pcb_t process_table[MAX_PROCESSES];

static uint32_t next_pid = 1;

void process_init(void) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = TERMINATED;
        process_table[i].esp = 0;
        process_table[i].eip = 0;
        process_table[i].next = NULL;
    }

    next_pid = 1;
}

pcb_t *process_create(void (*entry)(void)) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {

        if (process_table[i].state == TERMINATED) {

            pcb_t *proc = &process_table[i];

            proc->pid = next_pid++;
            proc->state = READY;

            proc->eip = (uint32_t)entry;

            proc->esp =
                (uint32_t)&proc->stack[STACK_SIZE / 4];

            proc->next = NULL;

            return proc;
        }
    }

    return NULL;
}

void process_yield(void) {
    /* Scheduler will be added later */
}

void process_exit(void) {
    /* Process termination will be added later */
}

pcb_t *process_get(uint32_t index) {
    if (index >= MAX_PROCESSES) {
        return NULL;
    }

    return &process_table[index];
}

uint32_t process_count(void) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid != 0 &&
            process_table[i].state != TERMINATED) {
            count++;
        }
    }

    return count;
}

const char *process_state_name(proc_state_t state) {
    switch (state) {
        case READY:
            return "READY";

        case RUNNING:
            return "RUNNING";

        case BLOCKED:
            return "BLOCKED";

        case TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}
