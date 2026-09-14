#ifndef TIMER_H
#define TIMER_H

#include "../include/types.h"

void timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
void timer_handler(void);

uint32_t timer_interrupt(uint32_t current_esp);

#endif
