#include "timer.h"
#include "io.h"
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
