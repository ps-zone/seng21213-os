#include "timer.h"
#include "io.h"
#include "scheduler.h"
#include "pic.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182

static volatile uint32_t timer_ticks = 0;

void timer_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    outb(PIT_COMMAND, 0x36);

    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Enable IRQ0 on the PIC */
    pic_unmask_irq(0);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_handler(void) {
    timer_ticks++;

    /* Tell the PIC that IRQ0 has been handled */
    pic_send_eoi(0);
}

uint32_t timer_interrupt(uint32_t current_esp) {
    timer_ticks++;

    /* Tell PIC that IRQ0 has been handled */
    pic_send_eoi(0);

    /*
     * While the scheduler is disabled,
     * continue using the current stack.
     */
    if (!scheduler_is_running()) {
        return current_esp;
    }

    /*
     * Scheduler is active:
     * save the old ESP and return the next process ESP.
     */
    return scheduler_switch(current_esp);
}
