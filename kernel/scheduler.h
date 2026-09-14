#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_add_process(pcb_t *process);

pcb_t *scheduler_next(void);
pcb_t *scheduler_current(void);

/*
 * Save the current process stack pointer and
 * return the next process stack pointer.
 */
uint32_t scheduler_switch(uint32_t current_esp);

void scheduler_start(void);
uint32_t scheduler_is_running(void);

#endif
