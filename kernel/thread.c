#include "thread.h"

static thread_t thread_table[MAX_THREADS];

static uint32_t next_tid = 1;

void thread_init(void) {

    for (uint32_t i = 0; i < MAX_THREADS; i++) {

        thread_table[i].tid = 0;
        thread_table[i].state = THREAD_TERMINATED;

        thread_table[i].esp = 0;
        thread_table[i].eip = 0;

        thread_table[i].entry = NULL;
        thread_table[i].arg = NULL;

        thread_table[i].next = NULL;
    }

    next_tid = 1;
}


thread_t *thread_create(
    void (*entry)(void *),
    void *arg
) {

    for (uint32_t i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].state == THREAD_TERMINATED) {

            thread_t *thread = &thread_table[i];

            thread->tid = next_tid++;
            thread->state = THREAD_READY;

            thread->entry = entry;
            thread->arg = arg;

            thread->eip = (uint32_t)entry;

            thread->esp =
                (uint32_t)&thread->stack[
                    THREAD_STACK_SIZE / sizeof(uint32_t)
                ];

            thread->next = NULL;

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

    for (uint32_t i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].tid != 0 &&
            thread_table[i].state != THREAD_TERMINATED) {

            count++;
        }
    }

    return count;
}


const char *thread_state_name(thread_state_t state) {

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
