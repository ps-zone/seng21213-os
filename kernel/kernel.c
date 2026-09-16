/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 – Foundations)
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
#include "pmm.h"
#include "fs.h"
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
static void cmd_mem(void);
static void cmd_ps(void);
static void cmd_ticks(void);
static void cmd_run(void);

static void cmd_threadtest(void);
static void cmd_racetest(void);
static void cmd_pctest(void);

static void cmd_meminfo(void);
static void cmd_memtest(void);
static void cmd_touch(const char *name);
static void cmd_ls(void);

static void cmd_write(const char *args);
static void cmd_cat(const char *name);
static void cmd_rm(const char *name);

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
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

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
    vga_puts_color("\n  SENG21213-OS Shell Commands\n",
                   VGA_YELLOW, VGA_BLACK);

    vga_puts("----------------------------------------\n");

    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");


    vga_puts_color("\n  Process Management:\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ps      - List active processes\n");
    vga_puts("  ticks   - Show timer tick count\n");
    vga_puts("  run     - Start round-robin scheduler\n");


    vga_puts_color("\n  Thread Management:\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  threadtest - Run Stage 2 thread test\n");
    vga_puts("  racetest   - Run race condition test\n");
    vga_puts("  pctest     - Run producer-consumer semaphore test\n");


    vga_puts_color("\n  Memory Management:\n",
		   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  meminfo - Show physical memory information\n");
    vga_puts("  memtest   - Test 100 frame allocations and frees\n");


    vga_puts_color("\n  File System:\n",
		   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ls      - List files\n");
    vga_puts("  touch   - Create an empty file\n");
    vga_puts("  write   - Write text to a file\n");
    vga_puts("  cat     - Print file contents\n");
    vga_puts("  rm      - Delete a file\n");


    vga_puts_color("\n  Future Milestones:\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  threads - [L10] List kernel threads\n");
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

static void cmd_meminfo(void) {
    uint32_t total = pmm_total_memory();
    uint32_t used  = pmm_used_memory();
    uint32_t free  = pmm_free_memory();

    vga_puts("\n  Physical Memory Information\n");
    vga_puts("  ---------------------------\n");

    vga_printf("  Total memory: %u MB\n",
               total / (1024 * 1024));

    vga_printf("  Used memory:  %u MB\n",
               used / (1024 * 1024));

    vga_printf("  Free memory:  %u MB\n",
               free / (1024 * 1024));

    vga_puts("\n");
}

static void cmd_memtest(void) {
    uint32_t frames[100];
    uint32_t free_before;
    uint32_t free_after_alloc;
    uint32_t free_after_free;

    vga_puts("\n  Stage 3 Physical Memory Test\n");
    vga_puts("  ----------------------------\n");

    free_before = pmm_free_memory();

    vga_puts("  Allocating 100 frames...\n");

    for (uint32_t i = 0; i < 100; i++) {
        frames[i] = pmm_alloc_frame();

        if (frames[i] == 0) {
            vga_printf("  ERROR: Allocation failed at frame %u\n", i);

            /*
             * Release frames that were already allocated.
             */
            for (uint32_t j = 0; j < i; j++) {
                pmm_free_frame(frames[j]);
            }

            return;
        }
    }

    free_after_alloc = pmm_free_memory();

    vga_puts("  100 frames allocated successfully.\n");
    vga_puts("  Freeing 100 frames...\n");

    for (uint32_t i = 0; i < 100; i++) {
        pmm_free_frame(frames[i]);
    }

    free_after_free = pmm_free_memory();

    vga_puts("  100 frames freed successfully.\n\n");

    vga_printf("  Free before:     %u KB\n",
               free_before / 1024);

    vga_printf("  After allocate:  %u KB\n",
               free_after_alloc / 1024);

    vga_printf("  After free:      %u KB\n",
               free_after_free / 1024);

    if (free_before == free_after_free) {
        vga_puts("\n  Result: SUCCESS - no memory leak detected!\n");
    } else {
        vga_puts("\n  Result: ERROR - memory leak detected!\n");
    }

    vga_puts("\n  Stage 3 memory test complete.\n\n");
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

/* --------------------------------------------------------------------------
 * Stage 4 - Create an empty file
 * -------------------------------------------------------------------------- */
static void cmd_touch(const char *name) {

    if (name == 0 || k_strlen(name) == 0) {
        vga_puts("  Usage: touch <filename>\n");
        return;
    }

    if (fs_create(name) == 0) {
        vga_puts("  File created: ");
        vga_puts(name);
        vga_puts("\n");
    } else {
        vga_puts_color(
            "  Error: could not create file.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}


/* --------------------------------------------------------------------------
 * Stage 4 - List files
 * -------------------------------------------------------------------------- */
static void cmd_ls(void) {

    uint32_t count = fs_file_count();

    if (count == 0) {
        vga_puts("  No files found.\n");
        return;
    }

    vga_puts("\n  Files\n");
    vga_puts("  ------------------------------\n");

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        const dir_entry_t *entry =
            fs_get_directory_entry(i);

        if (entry != 0 && entry->used) {
            vga_puts("  ");
            vga_puts(entry->name);
            vga_puts("\n");
        }
    }

    vga_puts("\n");
}

/* --------------------------------------------------------------------------
 * Stage 4 - Write text to a file
 * Usage: write <filename> <text>
 * -------------------------------------------------------------------------- */
static void cmd_write(const char *args) {

    if (args == 0 || k_strlen(args) == 0) {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    /*
     * Find the first space.
     * Everything before it is the filename.
     * Everything after it is the text.
     */
    uint32_t i = 0;

    while (args[i] != '\0' && args[i] != ' ') {
        i++;
    }

    if (args[i] == '\0') {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    char filename[FS_MAX_FILENAME];

    if (i == 0 || i >= FS_MAX_FILENAME) {
        vga_puts("  Error: invalid filename.\n");
        return;
    }

    for (uint32_t j = 0; j < i; j++) {
        filename[j] = args[j];
    }

    filename[i] = '\0';

    const char *text = k_ltrim(args + i + 1);

    if (k_strlen(text) == 0) {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    if (fs_write(filename, text) == 0) {
        vga_puts("  File written successfully.\n");
    } else {
        vga_puts_color(
            "  Error: could not write file.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}


/* --------------------------------------------------------------------------
 * Stage 4 - Display file contents
 * Usage: cat <filename>
 * -------------------------------------------------------------------------- */
static void cmd_cat(const char *name) {

    if (name == 0 || k_strlen(name) == 0) {
        vga_puts("  Usage: cat <filename>\n");
        return;
    }

    /*
     * Shell input is 256 bytes, so this is enough
     * for text written using the current shell.
     */
    char buffer[256];

    int bytes_read =
        fs_read(name, buffer, sizeof(buffer));

    if (bytes_read < 0) {
        vga_puts_color(
            "  Error: file not found or could not be read.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    vga_puts("  ");
    vga_puts(buffer);
    vga_puts("\n");
}

/* --------------------------------------------------------------------------
 * Stage 4 - Delete a file
 * Usage: rm <filename>
 * -------------------------------------------------------------------------- */
static void cmd_rm(const char *name) {

    if (name == 0 || k_strlen(name) == 0) {
        vga_puts("  Usage: rm <filename>\n");
        return;
    }

    if (fs_delete(name) == 0) {
        vga_puts("  File deleted: ");
        vga_puts(name);
        vga_puts("\n");
    } else {
        vga_puts_color(
            "  Error: file not found or could not be deleted.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
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
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }

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

	if (k_strcmp(cmd, "meminfo") == 0) {
    	cmd_meminfo();
    	continue;
	}

	if (k_strcmp(cmd, "memtest") == 0) {
    	cmd_memtest();
    	continue;
	}

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

	if (k_strncmp(cmd, "touch ", 6) == 0) {
    	cmd_touch(k_ltrim(cmd + 6));
    	continue;
	}

	if (k_strcmp(cmd, "ls") == 0) {
    	cmd_ls();
    	continue;
	}

	if (k_strncmp(cmd, "write ", 6) == 0) {
    	cmd_write(k_ltrim(cmd + 6));
    	continue;
	}

	if (k_strncmp(cmd, "cat ", 4) == 0) {
    	cmd_cat(k_ltrim(cmd + 4));
    	continue;
	}

	if (k_strncmp(cmd, "rm ", 3) == 0) {
    	cmd_rm(k_ltrim(cmd + 3));
    	continue;
	}


        /* Milestone stubs */
        if (k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ){

            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
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
    pmm_init();
    fs_init();

    stage1_process1 = process_create(test_process_1);
    stage1_process2 = process_create(test_process_2);

    /* Enable hardware interrupts */
    __asm__ __volatile__("sti");

    /* Show kernel splash screen and start shell */
    print_splash();
    shell_run();
}
