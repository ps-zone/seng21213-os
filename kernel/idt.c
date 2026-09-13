#include "idt.h"
extern void irq0_handler(void);

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

void idt_set_gate(
    uint8_t num,
    uint32_t handler,
    uint16_t selector,
    uint8_t type_attr
) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = type_attr;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    for (uint32_t i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    idt_set_gate(
    32,
    (uint32_t)irq0_handler,
    0x08,
    0x8E
    );

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    __asm__ __volatile__(
        "lidt %0"
        :
        : "m"(idt_ptr)
    );
}
