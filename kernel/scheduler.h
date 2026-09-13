#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void scheduler_init(void);

void scheduler_add_process(pcb_t *process);

pcb_t *scheduler_next(void);

pcb_t *scheduler_current(void);

#endif
