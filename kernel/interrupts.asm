[BITS 32]

[GLOBAL irq0_handler]
[EXTERN timer_interrupt]

irq0_handler:
    ; Save all general-purpose registers
    pushad

    ; ESP now points to the saved register frame.
    mov eax, esp

    ; Pass current ESP to C:
    ; timer_interrupt(current_esp)
    push eax

    call timer_interrupt

    ; Remove the C function argument from the OLD stack.
    add esp, 4

    ; EAX contains the stack pointer that should run next.
    mov esp, eax

    ; Restore registers from selected process stack.
    popad

    ; Restore EIP, CS and EFLAGS.
    iretd
