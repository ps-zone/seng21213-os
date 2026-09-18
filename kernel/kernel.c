/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 2 – Threads, Mutex & Semaphore)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "../include/types.h"

static pcb_t *stage1_process1 = NULL;
static pcb_t *stage1_process2 = NULL;

#define RACE_ITERATIONS 1000

static volatile int32_t myglobal = 0;
static volatile uint32_t race_done_1 = 0;
static volatile uint32_t race_done_2 = 0;
static mutex_t race_mutex;

#define BUFFER_SIZE 5
#define PRODUCER_ITEMS 10

static int32_t pc_buffer[BUFFER_SIZE];
static uint32_t pc_in = 0;
static uint32_t pc_out = 0;

static semaphore_t pc_empty;
static semaphore_t pc_full;
static semaphore_t pc_mutex;

static volatile uint32_t producer_done = 0;
static volatile uint32_t consumer_done = 0;
static volatile uint32_t pc_error = 0;

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_mem(void);
static void cmd_ps(void);
static void cmd_kill(const char *args);
static void cmd_ticks(void);
static void cmd_run(void);

static void cmd_threadtest(void);
static void cmd_racetest(void);
static void cmd_pctest(void);

static void test_thread(void *arg);
static void test_process_1(void);
static void test_process_2(void);

static void race_without_mutex(void *arg);
static void race_with_mutex(void *arg);
static void race_controller(void *arg);

static void producer_thread(void *arg);
static void consumer_thread(void *arg);
static void pc_controller(void *arg);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 2: Threads, Mutex & Semaphore", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("----------------------------------------\n");
    vga_puts("  help          - Show this help message\n");
    vga_puts("  clear         - Clear the screen\n");
    vga_puts("  echo <text>   - Echo text to screen\n");
    vga_puts("  version       - Show kernel version\n");
    vga_puts("  colour f b    - Change colours (0-15)\n");
    vga_puts("  halt          - Halt the CPU\n");
    vga_puts("  about         - About this OS and course\n");
    vga_puts("  mem           - Memory map (Stage 3 stub)\n");
    vga_puts_color("\n  Stage 1 - Process Management:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps            - List active processes\n");
    vga_puts("  kill <pid>    - Terminate a process\n");
    vga_puts("  ticks         - Show timer tick count\n");
    vga_puts("  run           - Start round-robin process demo\n");
    vga_puts_color("\n  Stage 2 - Threads & Synchronisation:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  threadtest    - Run kernel thread test\n");
    vga_puts("  racetest      - Race condition / mutex demo\n");
    vga_puts("  pctest        - Producer-consumer semaphore demo\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS Version 0.3 - Stage 2\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  Threads, mutexes and semaphores\n\n");
}

static void cmd_colour(const char *args) {
    const char *p = k_ltrim(args);
    uint32_t fg = 0, bg = 0;
    if (*p < '0' || *p > '9') { vga_puts("  Usage: colour <fg> <bg>\n"); return; }
    while (*p >= '0' && *p <= '9') { fg = fg * 10 + (uint32_t)(*p - '0'); p++; }
    p = k_ltrim(p);
    if (*p < '0' || *p > '9') { vga_puts("  Usage: colour <fg> <bg>\n"); return; }
    while (*p >= '0' && *p <= '9') { bg = bg * 10 + (uint32_t)(*p - '0'); p++; }
    p = k_ltrim(p);
    if (*p != '\0' || fg > 15 || bg > 15) {
        vga_puts("  Error: foreground and background must be 0-15.\n"); return;
    }
    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("  Colour changed.\n");
}

static void cmd_halt(void) {
    vga_puts("\n  System halted.\n");
    __asm__ __volatile__("cli");
    for (;;) __asm__ __volatile__("hlt");
}

static void cmd_mem(void) {
    /* Stage 0 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub – implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    – 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

static void cmd_ps(void) {
    vga_puts_color("\n  Process List\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  PID   STATE\n");
    vga_puts("  ----------------\n");

    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {

        pcb_t *proc = process_get(i);

        if (proc != NULL &&
            proc->pid != 0 &&
            proc->state != TERMINATED) {

            vga_printf("  %u     %s\n",
                       proc->pid,
                       process_state_name(proc->state));

            count++;
        }
    }

    if (count == 0) {
        vga_puts("  No active processes.\n");
    }

    vga_printf("\n  Total active processes: %u\n\n", count);
}

static void cmd_kill(const char *args) {
    const char *p = k_ltrim(args);
    uint32_t pid = 0;
    if (*p < '0' || *p > '9') { vga_puts("  Usage: kill <pid>\n"); return; }
    while (*p >= '0' && *p <= '9') { pid = pid * 10 + (uint32_t)(*p - '0'); p++; }
    p = k_ltrim(p);
    if (*p != '\0' || pid == 0) { vga_puts("  Usage: kill <pid>\n"); return; }
    if (process_kill(pid)) vga_printf("  Process %u terminated.\n", pid);
    else vga_printf("  Error: process %u not found or already terminated.\n", pid);
}

static void cmd_ticks(void) {
    vga_printf("\n  Timer ticks: %u\n\n", timer_get_ticks());
}

static void cmd_run(void) {
    /*
     * P1 and P2 were already created during kernel startup.
     * Now add them to the ready queue and start scheduling.
     */
    if (stage1_process1 == NULL || stage1_process2 == NULL) {
        vga_puts("  Failed to create Stage 1 test processes.\n");
        return;
    }

    scheduler_add_process(stage1_process1);
    scheduler_add_process(stage1_process2);

    vga_puts("\n  Starting Stage 1 process scheduler...\n");

    scheduler_start();

    while (true) {
        __asm__ __volatile__("hlt");
    }
}


static void cmd_threadtest(void) {
    vga_puts("\n  Creating two kernel threads...\n");

    thread_t *t1 = thread_create(
        test_thread,
        "T1 "
    );

    thread_t *t2 = thread_create(
        test_thread,
        "T2 "
    );

    if (t1 == NULL || t2 == NULL) {
        vga_puts("  Failed to create threads.\n\n");
        return;
    }

    vga_puts("  Threads created successfully.\n");
    vga_printf("  TID %u -> T1\n", t1->tid);
    vga_printf("  TID %u -> T2\n", t2->tid);
    vga_puts("  Starting scheduler...\n\n");

    scheduler_start();

    while (true) {
        __asm__ __volatile__("hlt");
    }
}

static void cmd_racetest(void) {
    vga_puts("\n  Stage 2 Race Condition Test\n");
    vga_puts("  ---------------------------\n");

    myglobal = 0;
    race_done_1 = 0;
    race_done_2 = 0;

    vga_puts("\n  Test 1: WITHOUT mutex\n");
    vga_puts("  Two threads will increment myglobal.\n");
    vga_puts("  Expected final value: 2000\n\n");

    thread_t *t1 = thread_create(race_without_mutex, (void *)1);
    thread_t *t2 = thread_create(race_without_mutex, (void *)2);
    thread_t *controller = thread_create(race_controller, NULL);
    
    if (t1 == NULL || t2 == NULL || controller == NULL){
        vga_puts("  Failed to create race-test threads.\n");
        return;
    }

    scheduler_start();

    while (true) {
        __asm__ __volatile__("hlt");
    }
}

static void cmd_pctest(void) {
    vga_puts("\n  Stage 2 Producer-Consumer Test\n");
    vga_puts("  ------------------------------\n");

    /* Reset shared buffer state */
    pc_in = 0;
    pc_out = 0;
    producer_done = 0;
    consumer_done = 0;
    pc_error = 0;

    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        pc_buffer[i] = 0;
    }

    /*
     * Three semaphores:
     * empty = number of empty buffer slots
     * full  = number of filled buffer slots
     * mutex = binary semaphore protecting the buffer
     */
    sem_init(&pc_empty, BUFFER_SIZE);
    sem_init(&pc_full, 0);
    sem_init(&pc_mutex, 1);

    vga_puts("  Buffer size: 5\n");
    vga_puts("  Items to produce: 10\n");
    vga_puts("  Semaphores: empty=5, full=0, mutex=1\n\n");

    thread_t *producer =
        thread_create(producer_thread, NULL);

    thread_t *consumer =
        thread_create(consumer_thread, NULL);

    thread_t *controller =
        thread_create(pc_controller, NULL);

    if (producer == NULL ||
        consumer == NULL ||
        controller == NULL) {

        vga_puts("  Failed to create producer-consumer threads.\n");
        return;
    }

    vga_puts("  Starting producer and consumer...\n\n");

    scheduler_start();

    while (true) {
        __asm__ __volatile__("hlt");
    }
}

static void test_process_1(void) {
    uint32_t last_tick = 0;

    while (true) {
        uint32_t now = timer_get_ticks();

        if (now - last_tick >= 50) {
            vga_puts_color("P1 ", VGA_LIGHT_GREEN, VGA_BLACK);
            last_tick = now;
        }
    }
}

static void test_process_2(void) {
    uint32_t last_tick = 0;

    while (true) {
        uint32_t now = timer_get_ticks();

        if (now - last_tick >= 50) {
            vga_puts_color("P2 ", VGA_LIGHT_CYAN, VGA_BLACK);
            last_tick = now;
        }
    }
}

static void test_thread(void *arg) {
    const char *name = (const char *)arg;

    while (true) {
        vga_puts(name);

        for (volatile uint32_t i = 0; i < 1000000; i++) {
            /* Small delay */
        }
    }
}

static void race_without_mutex(void *arg) {
    uint32_t worker_id = (uint32_t)arg;

    for (uint32_t i = 0; i < RACE_ITERATIONS; i++) {

        int32_t temp = myglobal;

        /*
         * Give the timer enough opportunity to switch threads
         * between reading and writing myglobal.
         */
        for (volatile uint32_t delay = 0; delay < 50000; delay++) {
            __asm__ __volatile__("nop");
        }

        temp++;
        myglobal = temp;
    }

    if (worker_id == 1) {
        race_done_1 = 1;
    } else {
        race_done_2 = 1;
    }
}

static void race_with_mutex(void *arg) {
    uint32_t worker_id = (uint32_t)arg;

    for (uint32_t i = 0; i < RACE_ITERATIONS; i++) {

        mutex_lock(&race_mutex);

        int32_t temp = myglobal;

        for (volatile uint32_t delay = 0; delay < 50000; delay++) {
            __asm__ __volatile__("nop");
        }

        temp++;
        myglobal = temp;

        mutex_unlock(&race_mutex);
    }

    if (worker_id == 1) {
        race_done_1 = 1;
    } else {
        race_done_2 = 1;
    }
}

static void race_controller(void *arg) {
    (void)arg;

    /* Wait for both WITHOUT-mutex workers */
    while (!race_done_1 || !race_done_2) {
        __asm__ __volatile__("hlt");
    }

    vga_puts("\n\n  WITHOUT mutex result\n");
    vga_puts("  Expected value: 2000\n");
    vga_printf("  Actual value:   %d\n", myglobal);

    if (myglobal == (RACE_ITERATIONS * 2)) {
        vga_puts("  Result: No lost updates detected.\n");
    } else {
        vga_puts("  Result: RACE CONDITION detected!\n");
    }

    /* Prepare second test */
    myglobal = 0;
    race_done_1 = 0;
    race_done_2 = 0;

    mutex_init(&race_mutex);

    vga_puts("\n  Test 2: WITH mutex\n");
    vga_puts("  Two threads will increment myglobal safely.\n");
    vga_puts("  Expected final value: 2000\n\n");

    thread_t *t1 =
        thread_create(race_with_mutex, (void *)1);

    thread_t *t2 =
        thread_create(race_with_mutex, (void *)2);

    if (t1 == NULL || t2 == NULL) {
        vga_puts("  Failed to create mutex race-test threads.\n");
        return;
    }

    /* Wait for both protected workers */
    while (!race_done_1 || !race_done_2) {
        __asm__ __volatile__("hlt");
    }

    vga_puts("\n  WITH mutex result\n");
    vga_puts("  Expected value: 2000\n");
    vga_printf("  Actual value:   %d\n", myglobal);

    if (myglobal == (RACE_ITERATIONS * 2)) {
        vga_puts("  Result: CORRECT - mutex prevented the race condition!\n");
    } else {
        vga_puts("  Result: ERROR - incorrect protected result.\n");
    }

    vga_puts("\n  Stage 2 race demonstration complete.\n");
}

static void producer_thread(void *arg) {
    (void)arg;

    for (uint32_t i = 1; i <= PRODUCER_ITEMS; i++) {

        /* Wait until there is an empty buffer slot */
        sem_wait(&pc_empty);

        /* Lock the buffer */
        sem_wait(&pc_mutex);

        pc_buffer[pc_in] = (int32_t)i;

        vga_printf("  Producer -> %u\n", i);

        pc_in = (pc_in + 1) % BUFFER_SIZE;

        /* Unlock the buffer */
        sem_signal(&pc_mutex);

        /* One more filled slot is now available */
        sem_signal(&pc_full);
    }

    producer_done = 1;
}

static void consumer_thread(void *arg) {
    (void)arg;

    for (uint32_t expected = 1;
         expected <= PRODUCER_ITEMS;
         expected++) {

        /* Wait until an item is available */
        sem_wait(&pc_full);

        /* Lock the buffer */
        sem_wait(&pc_mutex);

        int32_t item = pc_buffer[pc_out];

        pc_out = (pc_out + 1) % BUFFER_SIZE;

        vga_printf("  Consumer <- %u\n", (uint32_t)item);

        /*
         * Check that items are consumed
         * in the correct order.
         */
        if (item != (int32_t)expected) {
            pc_error = 1;
        }

        /* Unlock the buffer */
        sem_signal(&pc_mutex);

        /* One more empty slot is now available */
        sem_signal(&pc_empty);
    }

    consumer_done = 1;
}

static void pc_controller(void *arg) {
    (void)arg;

    /* Wait until both producer and consumer finish */
    while (!producer_done || !consumer_done) {
        __asm__ __volatile__("hlt");
    }

    vga_puts("\n  Producer-Consumer Test Result\n");
    vga_puts("  -----------------------------\n");

    if (pc_error == 0) {
        vga_puts("  Result: SUCCESS - buffer completed without corruption!\n");
    } else {
        vga_puts("  Result: ERROR - buffer corruption detected!\n");
    }

    vga_puts("\n  Stage 2 producer-consumer demonstration complete.\n");
}

/*-----------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about")   == 0) { cmd_about();   continue; }
        if (k_strcmp(cmd, "mem")     == 0) { cmd_mem();     continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }
	
	if (k_strcmp(cmd, "ps") == 0) {
    	cmd_ps();
    	continue;
	}

	if (k_strcmp(cmd, "ticks") == 0) {
	cmd_ticks();
	continue;
	}

	if (k_strcmp(cmd, "run") == 0) {
	cmd_run();
	continue;
	}

	if (k_strcmp(cmd, "threadtest") == 0) {
	cmd_threadtest();
	continue;
	}

	if (k_strcmp(cmd, "racetest") == 0) {
	cmd_racetest();
	continue;
	}

	if (k_strcmp(cmd, "pctest") == 0) {
	cmd_pctest();
	continue;
	}

        if (k_strcmp(cmd, "echo") == 0) { cmd_echo(""); continue; }
        if (k_strncmp(cmd, "echo ", 5) == 0) { cmd_echo(k_ltrim(cmd + 5)); continue; }
        if (k_strcmp(cmd, "colour") == 0) { cmd_colour(""); continue; }
        if (k_strncmp(cmd, "colour ", 7) == 0) { cmd_colour(cmd + 7); continue; }
        if (k_strcmp(cmd, "kill") == 0) { cmd_kill(""); continue; }
        if (k_strncmp(cmd, "kill ", 5) == 0) { cmd_kill(cmd + 5); continue; }

        if (k_strcmp(cmd, "free") == 0 || k_strcmp(cmd, "ls") == 0 ||
            k_strcmp(cmd, "cat") == 0) {
            vga_puts_color("  [TODO] This command belongs to a later stage.\n", VGA_YELLOW, VGA_BLACK);
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
/* -------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * ------------------------------------------------------------------------- */
void kernel_main(void) {
    /* Basic hardware initialization */
    vga_init();
    kb_init();

    /* Interrupt and timer initialization */
    idt_init();
    pic_remap();
    timer_init(100);

    /* Process, scheduler, and thread initialization */
    process_init();
    scheduler_init();
    thread_init();

    stage1_process1 = process_create(test_process_1);
    stage1_process2 = process_create(test_process_2);

    /* Enable hardware interrupts */
    __asm__ __volatile__("sti");

    /* Show kernel splash screen and start shell */
    print_splash();
    shell_run();
}
