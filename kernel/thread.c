#include "thread.h"
#include "scheduler.h"

static thread_t thread_table[MAX_THREADS];

static uint32_t next_tid = 1;


/*
 * This function is the first code executed by a new thread.
 *
 * EAX contains a pointer to the thread structure.
 */
static void thread_bootstrap(void) {

    thread_t *thread;

    __asm__ __volatile__(
        "movl %%eax, %0"
        : "=r"(thread)
    );

    thread->state = THREAD_RUNNING;

    /*
     * Run the real thread function with its argument.
     */
    thread->entry(thread->arg);

    /*
     * If the thread function returns, mark it terminated.
     */
    thread->state = THREAD_TERMINATED;
    thread->context.state = TERMINATED;

    while (1) {
        __asm__ __volatile__("hlt");
    }
}


void thread_init(void) {

    for (uint32_t i = 0; i < MAX_THREADS; i++) {

        thread_table[i].tid = 0;

        thread_table[i].state =
            THREAD_TERMINATED;

        thread_table[i].entry = NULL;
        thread_table[i].arg = NULL;

        thread_table[i].next = NULL;

        thread_table[i].context.pid = 0;
        thread_table[i].context.state = TERMINATED;
        thread_table[i].context.esp = 0;
        thread_table[i].context.eip = 0;
        thread_table[i].context.next = NULL;
    }

    next_tid = 1;
}


thread_t *thread_create(
    void (*entry)(void *),
    void *arg
) {

    for (uint32_t i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].state ==
            THREAD_TERMINATED) {

            thread_t *thread =
                &thread_table[i];

            thread->tid = next_tid++;
            thread->state = THREAD_READY;

            thread->entry = entry;
            thread->arg = arg;

            /*
             * Give the scheduler a unique ID.
             *
             * 1000+ is used so thread contexts are easy
             * to distinguish from normal Stage 1 PIDs.
             */
            thread->context.pid =
                1000 + thread->tid;

            thread->context.state = READY;

            thread->context.eip =
                (uint32_t)thread_bootstrap;

            thread->context.next = NULL;

            /*
             * Build the same initial CPU stack frame
             * used by our Stage 1 process scheduler.
             */
            uint32_t *stack =
                &thread->context.stack[
                    STACK_SIZE / sizeof(uint32_t)
                ];

            /* IRETD frame */
            *(--stack) = 0x202;                    /* EFLAGS */
            *(--stack) = 0x08;                     /* CS */
            *(--stack) = (uint32_t)thread_bootstrap; /* EIP */

            /*
             * POPAD frame
             *
             * EAX contains the thread pointer so
             * thread_bootstrap() can find fn + arg.
             */
            *(--stack) = (uint32_t)thread; /* EAX */
            *(--stack) = 0;                /* ECX */
            *(--stack) = 0;                /* EDX */
            *(--stack) = 0;                /* EBX */
            *(--stack) = 0;                /* ignored ESP */
            *(--stack) = 0;                /* EBP */
            *(--stack) = 0;                /* ESI */
            *(--stack) = 0;                /* EDI */

            thread->context.esp =
                (uint32_t)stack;

            /*
             * Add this execution context to our existing
             * Stage 1 round-robin scheduler.
             */
            scheduler_add_process(
                &thread->context
            );

            return thread;
        }
    }

    return NULL;
}


thread_t *thread_get(uint32_t index) {

    if (index >= MAX_THREADS) {
        return NULL;
    }

    return &thread_table[index];
}


uint32_t thread_count(void) {

    uint32_t count = 0;

    for (uint32_t i = 0;
         i < MAX_THREADS;
         i++) {

        if (thread_table[i].tid != 0 &&
            thread_table[i].state !=
                THREAD_TERMINATED) {

            count++;
        }
    }

    return count;
}


const char *thread_state_name(
    thread_state_t state
) {

    switch (state) {

        case THREAD_READY:
            return "READY";

        case THREAD_RUNNING:
            return "RUNNING";

        case THREAD_BLOCKED:
            return "BLOCKED";

        case THREAD_TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}
