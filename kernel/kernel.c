#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "../include/types.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char *args);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_ps(void);
static void cmd_kill(const char *args);
static void cmd_ticks(void);
static void cmd_run(void);

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

static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

static void print_splash(void) {
    vga_clear(VGA_BLACK);
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);
    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 1: Process Management", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);
    vga_set_cursor(4, 2);
    vga_puts_color("  PCB + 100 Hz PIT + Round-Robin Scheduler",
                   VGA_LIGHT_GREEN, VGA_BLACK);
    vga_set_cursor(5, 2);
    vga_puts_color("  Type 'help' to see available commands.",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
}

static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------------------------------\n");
    vga_puts("  help             - Show this help message\n");
    vga_puts("  clear            - Clear the screen\n");
    vga_puts("  echo <text>      - Echo text to screen\n");
    vga_puts("  version          - Show OS version\n");
    vga_puts("  colour <fg> <bg> - Change shell colours (0-15)\n");
    vga_puts("  halt             - Halt the CPU\n");

    vga_puts_color("\n  Process Management:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps               - List active processes\n");
    vga_puts("  kill <pid>       - Terminate a process\n");
    vga_puts("  ticks            - Show timer tick count\n");
    vga_puts("  run              - Start round-robin scheduler\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS Version 0.2\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  Stage 1 - Process Management\n\n");
}

static void cmd_colour(const char *args) {
    const char *p = k_ltrim(args);
    uint32_t fg = 0;
    uint32_t bg = 0;

    if (*p < '0' || *p > '9') {
        vga_puts("  Usage: colour <fg> <bg>\n");
        vga_puts("  Colours: 0-15\n");
        return;
    }

    while (*p >= '0' && *p <= '9') {
        fg = fg * 10 + (uint32_t)(*p - '0');
        p++;
    }

    p = k_ltrim(p);
    if (*p < '0' || *p > '9') {
        vga_puts("  Usage: colour <fg> <bg>\n");
        vga_puts("  Colours: 0-15\n");
        return;
    }

    while (*p >= '0' && *p <= '9') {
        bg = bg * 10 + (uint32_t)(*p - '0');
        p++;
    }

    p = k_ltrim(p);
    if (*p != '\0' || fg > 15 || bg > 15) {
        vga_puts("  Error: foreground and background must be 0-15.\n");
        return;
    }

    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("  Colour changed.\n");
}

static void cmd_halt(void) {
    vga_puts("\n  System halted.\n");
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void cmd_ps(void) {
    vga_puts_color("\n  Process List\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  PID   STATE\n");
    vga_puts("  ----------------\n");

    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *proc = process_get(i);
        if (proc != NULL && proc->pid != 0 && proc->state != TERMINATED) {
            vga_printf("  %u     %s\n", proc->pid, process_state_name(proc->state));
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

    if (*p < '0' || *p > '9') {
        vga_puts("  Usage: kill <pid>\n");
        return;
    }

    while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (uint32_t)(*p - '0');
        p++;
    }

    p = k_ltrim(p);
    if (*p != '\0' || pid == 0) {
        vga_puts("  Usage: kill <pid>\n");
        return;
    }

    if (process_kill(pid)) {
        vga_printf("  Process %u terminated.\n", pid);
    } else {
        vga_printf("  Error: process %u not found or already terminated.\n", pid);
    }
}

static void cmd_ticks(void) {
    vga_printf("\n  Timer ticks: %u\n\n", timer_get_ticks());
}

static void cmd_run(void) {
    if (process_count() == 0) {
        vga_puts("  No active processes to schedule.\n");
        return;
    }

    vga_puts("\n  Starting round-robin scheduler...\n");
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

static char shell_buf[256];
static char prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        if (k_strcmp(cmd, "help") == 0)    { cmd_help(); continue; }
        if (k_strcmp(cmd, "clear") == 0)   { cmd_clear(); continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt") == 0)    { cmd_halt(); continue; }
        if (k_strcmp(cmd, "ps") == 0)      { cmd_ps(); continue; }
        if (k_strcmp(cmd, "ticks") == 0)   { cmd_ticks(); continue; }
        if (k_strcmp(cmd, "run") == 0)     { cmd_run(); continue; }

        if (k_strcmp(cmd, "echo") == 0) {
            cmd_echo("");
            continue;
        }
        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "colour") == 0) {
            cmd_colour("");
            continue;
        }
        if (k_strncmp(cmd, "colour ", 7) == 0) {
            cmd_colour(cmd + 7);
            continue;
        }

        if (k_strcmp(cmd, "kill") == 0) {
            cmd_kill("");
            continue;
        }
        if (k_strncmp(cmd, "kill ", 5) == 0) {
            cmd_kill(cmd + 5);
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

void kernel_main(void) {
    vga_init();
    kb_init();

    idt_init();
    pic_remap();
    timer_init(100);

    __asm__ __volatile__("sti");

    process_init();
    scheduler_init();

    pcb_t *process1 = process_create(test_process_1);
    pcb_t *process2 = process_create(test_process_2);

    scheduler_add_process(process1);
    scheduler_add_process(process2);

    print_splash();
    shell_run();

    __asm__ __volatile__("hlt");
}
