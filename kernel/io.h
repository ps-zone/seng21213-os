#ifndef IO_H
#define IO_H

#include "../include/types.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/* Small delay used when communicating with old hardware */
static inline void io_wait(void) {
    __asm__ __volatile__(
        "outb %%al, $0x80"
        :
        : "a"(0)
    );
}

#endif
