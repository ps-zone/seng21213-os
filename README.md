# SENG21213-OS — x86 Operating System

**Course:** SENG 21213 – Computer Architecture & Operating Systems  
**Programme:** BSc (Hons) in Software Engineering  
**University:** University of Kelaniya  

---

## Project Overview

SENG21213-OS is a small educational x86 operating system developed as part of the
SENG 21213 – Computer Architecture & Operating Systems assignment.

The project was developed progressively through five stages, starting with a
bootable kernel and extending it with process management, threads and
synchronisation, physical memory management, and a RAM-disk-based file system.

The operating system runs in 32-bit x86 protected mode and provides a simple
interactive shell.

---

## Implemented Stages

| Stage | Description | Status |
|------|-------------|--------|
| Stage 0 | Kernel Foundations – Bootloader, VGA, Keyboard and Shell | Completed |
| Stage 1 | Process Management and Scheduling | Completed |
| Stage 2 | Threads and Synchronisation | Completed |
| Stage 3 | Physical Memory Management | Completed |
| Stage 4 | RAM Disk File System | Completed |

---

## Main Features

### Stage 0 – Kernel Foundations

- x86 bootloader written in NASM
- 16-bit real mode to 32-bit protected mode transition
- Global Descriptor Table (GDT)
- VGA text-mode output
- PS/2 keyboard input
- Interactive kernel shell

### Stage 1 – Process Management

- Process Control Blocks (PCB)
- Process creation
- Process states
- Process scheduling
- Context switching
- Timer-based scheduling
- Process information display

### Stage 2 – Threads and Synchronisation

- Kernel threads
- Thread scheduling
- Mutex implementation
- Counting semaphore implementation
- Race-condition demonstration
- Producer-consumer synchronisation test

### Stage 3 – Physical Memory Management

- BIOS E820 memory map support
- 4 KB physical memory frames
- Bitmap-based physical memory management
- Physical frame allocation
- Physical frame deallocation
- Memory information command
- Memory allocation/deallocation test
- Reserved memory region for the Stage 4 RAM disk

### Stage 4 – RAM Disk File System

- 1 MB RAM disk
- 4 KB blocks
- File-system metadata
- Inode-based file representation
- Direct data blocks
- Flat directory structure
- File creation
- File reading
- File writing
- File deletion
- File descriptor operations
- Multiple file support

---

## File System Commands

The Stage 4 file system can be accessed through the kernel shell.

### List files

```text
ls
