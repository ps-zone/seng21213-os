# SENG21213-OS — Stage 2: Threads, Mutex & Semaphore

Educational 32-bit x86 operating system for SENG 21213 — Computer Architecture & Operating Systems, University of Kelaniya.

## Implemented in Stage 2
- `thread_create(fn, arg)` kernel threads sharing the same address space.
- Blocking `mutex_t` with `mutex_lock()` / `mutex_unlock()`.
- Counting `semaphore_t` with `sem_wait()` / `sem_signal()`.
- `myglobal` race-condition demonstration without and with mutex protection.
- Bounded-buffer producer-consumer using three semaphores: `empty`, `full`, `mutex`.
- Stage 1 100 Hz PIT and round-robin scheduling retained.
- Stage 0 shell commands retained.

## Build and run
```bash
make clean && make
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

## Commands
Stage 0: `help`, `clear`, `echo <text>`, `version`, `colour <fg> <bg>`, `halt`.

Stage 1: `ps`, `kill <pid>`, `ticks`, `run`.

Stage 2:
- `threadtest` — creates and schedules two kernel threads.
- `racetest` — shows the `myglobal` race without a mutex and the protected result with a mutex.
- `pctest` — bounded-buffer producer-consumer test using `empty`, `full`, and `mutex` semaphores.

Use a fresh QEMU boot for `run`, `threadtest`, `racetest`, and `pctest` because each starts the scheduler.

## Stage files
Stage 1 context switch: `boot/switch.asm`.

Stage 2: `kernel/thread.c`, `kernel/thread.h`, `kernel/mutex.c`, `kernel/mutex.h`, `kernel/semaphore.c`, `kernel/semaphore.h`.

## Submission
Stage 2 tag: `v0.3-stage2`.

Before tagging, confirm `make clean && make` has zero warnings/errors and verify all Stage 2 demos in QEMU.
