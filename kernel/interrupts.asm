[BITS 32]

[GLOBAL irq0_handler]
[EXTERN timer_handler]

irq0_handler:
    pushad

    call timer_handler

    popad
    iretd

