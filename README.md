# SENG21213-OS — x86 Operating System

**Course:** SENG 21213 – Computer Architecture & Operating Systems  
**Programme:** BSc (Hons) in Software Engineering  
**University:** University of Kelaniya  

---

## 1. Project Overview

SENG21213-OS is a small educational **32-bit x86 operating system** developed for the SENG 21213 – Computer Architecture & Operating Systems assignment.

The purpose of this project is to understand important operating-system concepts by building them step by step. The project starts with a bootable kernel and gradually adds processes, scheduling, threads, synchronisation, physical memory management, and a RAM-disk file system.

The system boots into 32-bit protected mode and provides an interactive kernel shell that can be used to test the implemented features.

---

## 2. Project Goals

This project demonstrates:

- How an x86 computer boots a small operating system
- How the CPU changes from 16-bit real mode to 32-bit protected mode
- Basic VGA text output and keyboard input
- Process creation and CPU scheduling
- Kernel threads
- Race conditions and synchronisation
- Mutexes and counting semaphores
- Physical memory management
- A simple RAM-based file system
- How separate kernel components work together

---

## 3. Implemented Stages

| Stage | Description | Status |
|---|---|---|
| Stage 0 | Kernel Foundations – Bootloader, VGA, Keyboard and Shell | Completed |
| Stage 1 | Process Management and Scheduling | Completed |
| Stage 2 | Threads and Synchronisation | Completed |
| Stage 3 | Physical Memory Management | Completed |
| Stage 4 | RAM Disk File System | Completed |

---

## 4. Stage 0 — Kernel Foundations

Stage 0 provides the basic environment required by the rest of the operating system.

### Main Features

- x86 bootloader written in NASM
- 16-bit real mode to 32-bit protected mode transition
- Global Descriptor Table (GDT)
- Protected-mode kernel entry
- VGA 80×25 text-mode output
- PS/2 keyboard input
- Interactive kernel shell

### Basic Boot Flow

```text
Power On
   |
   v
BIOS
   |
   v
boot/boot.asm
   |
   |-- Loads the kernel
   |-- Sets up the GDT
   |-- Enters 32-bit protected mode
   v
kernel/kernel_entry.asm
   |
   v
kernel_main()
   |
   |-- Initialises kernel components
   v
Interactive Kernel Shell
```

---

## 5. Stage 1 — Process Management and Scheduling

Stage 1 adds basic process management.

A **process** represents a task managed by the operating system. Information about each process is stored using a Process Control Block (PCB).

### Main Features

- Process Control Blocks
- Process creation
- Process IDs
- Process states
- Ready and running process management
- Process scheduling
- Context switching
- Timer-based scheduling
- Manual CPU yielding
- Process information display

### Useful Commands

```text
ps
run
yield
```

`ps` displays process information. `run` demonstrates process scheduling, and `yield` allows execution to be passed to another scheduled task.

---

## 6. Stage 2 — Threads and Synchronisation

Stage 2 introduces kernel threads and synchronisation.

When multiple threads access shared data at the same time, the final result can become incorrect. This is a **race condition**. The project demonstrates this problem and uses mutexes and semaphores to control shared access.

### Main Features

- Kernel threads
- Thread scheduling
- Mutex implementation
- Counting semaphore implementation
- Race-condition demonstration
- Producer-consumer synchronisation

### Thread Test

```text
threadtest
```

Demonstrates execution between multiple kernel threads.

### Race Condition Test

```text
racetest
```

Demonstrates shared-data access without protection and then with mutex protection.

### Producer-Consumer Test

```text
pctest
```

Demonstrates producer-consumer synchronisation using counting semaphores.

---

## 7. Stage 3 — Physical Memory Management

Stage 3 adds a Physical Memory Manager (PMM).

Physical memory is divided into **4 KB frames**. The kernel tracks which frames are available or reserved and can allocate and free frames when required.

### Main Features

- BIOS E820 memory map support
- 4 KB physical memory frames
- Bitmap-based physical memory management
- Physical frame allocation
- Physical frame deallocation
- Memory information
- Allocation/deallocation testing
- Reserved memory region for the Stage 4 RAM disk

### Memory Information

```text
meminfo
```

Displays physical memory information.

### Memory Test

```text
memtest
```

Allocates frames, checks memory usage, frees the frames, and verifies that the memory becomes available again.

---

## 8. Stage 4 — RAM Disk File System

Stage 4 adds a simple file system stored in RAM.

The project uses a **1 MB RAM disk** divided into **4 KB blocks**. Files use inode information and a flat directory structure. The implementation is intentionally small so the main file-system concepts are easy to understand.

### Main Features

- 1 MB RAM disk
- 4 KB blocks
- File-system metadata
- Inode-based file representation
- Direct data blocks
- Flat directory structure
- File creation
- File opening and closing
- File reading
- File writing and appending
- File deletion
- File descriptor operations
- Multiple file support

---

## 9. File System Commands

### List Files

```text
ls
```

Displays the files currently stored in the RAM disk.

### Create a File

```text
touch file1.txt
```

Creates an empty file.

### Write to a File

```text
write file1.txt Hello
```

Writes text to the file. Additional writes append text to the existing contents.

### Read a File

```text
cat file1.txt
```

Displays the contents of the file.

### Delete a File

```text
rm file1.txt
```

Deletes the file.

---

## 10. Shell Command Reference

| Command | Description |
|---|---|
| `help` | Display available commands |
| `clear` | Clear the screen |
| `about` | Display operating-system information |
| `echo` | Print text |
| `ps` | Display process information |
| `run` | Run the process scheduling demonstration |
| `yield` | Yield CPU execution |
| `threadtest` | Test thread scheduling |
| `racetest` | Demonstrate a race condition and mutex protection |
| `pctest` | Run the producer-consumer semaphore test |
| `meminfo` | Display physical memory information |
| `memtest` | Test physical memory allocation/deallocation |
| `ls` | List files |
| `touch` | Create an empty file |
| `write` | Write or append text to a file |
| `cat` | Display file contents |
| `rm` | Delete a file |

Commands are entered at the kernel prompt:

```text
ksh>
```

---

## 11. Project Structure

```text
seng21213-os/
├── boot/
│   └── boot.asm
├── kernel/
│   ├── kernel_entry.asm
│   ├── kernel.c
│   ├── vga.c / vga.h
│   ├── keyboard.c / keyboard.h
│   ├── process.c / process.h
│   ├── scheduler.c / scheduler.h
│   ├── thread.c / thread.h
│   ├── mutex.c / mutex.h
│   ├── semaphore.c / semaphore.h
│   ├── pmm.c / pmm.h
│   ├── ramdisk.c / ramdisk.h
│   └── fs.c / fs.h
├── include/
│   └── types.h
├── linker.ld
├── Makefile
├── Dockerfile
├── .gitignore
└── README.md
```

### Important Files

- `boot/boot.asm` — starts the boot process and loads the kernel.
- `kernel/kernel_entry.asm` — provides the protected-mode kernel entry.
- `kernel/kernel.c` — contains the main kernel and shell logic.
- `kernel/vga.c` — provides VGA text output.
- `kernel/keyboard.c` — provides keyboard input.
- `kernel/process.c` and `scheduler.c` — provide process management and scheduling.
- `kernel/thread.c` — provides kernel thread support.
- `kernel/mutex.c` and `semaphore.c` — provide synchronisation.
- `kernel/pmm.c` — provides physical memory management.
- `kernel/ramdisk.c` — provides RAM-disk block storage.
- `kernel/fs.c` — provides file-system operations.
- `linker.ld` — controls the kernel memory layout.
- `Makefile` — builds the operating-system image.

---

## 12. Development Requirements

The project can be built using Linux or WSL2.

### Required Tools

- NASM
- GCC with 32-bit support
- GNU Binutils
- GNU Make
- QEMU
- Git

On Ubuntu/WSL2:

```bash
sudo apt update
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make
```

---

## 13. Building the Operating System

### Step 1 — Clean Previous Build Files

```bash
make clean
```

### Step 2 — Build

```bash
make all
```

A successful build creates:

```text
seng21213-os.img
```

---

## 14. Running with QEMU

Run the operating system using:

```bash
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

After the operating system boots, the interactive kernel shell appears.

```text
ksh>
```

Use:

```text
help
```

to see the available commands.

---

## 15. Example Stage 4 File System Test

Create five files:

```text
touch file1.txt
touch file2.txt
touch file3.txt
touch file4.txt
touch file5.txt
```

Check them:

```text
ls
```

Write some data:

```text
write file1.txt Hello
write file2.txt OperatingSystem
write file3.txt Kernel
write file4.txt Memory
write file5.txt FileSystem
```

Read the files:

```text
cat file1.txt
cat file2.txt
cat file3.txt
cat file4.txt
cat file5.txt
```

Test appending:

```text
write file1.txt World
cat file1.txt
```

Delete a file:

```text
rm file3.txt
```

Confirm the deletion:

```text
ls
```

This simple test checks file creation, listing, writing, appending, reading, and deletion.

---

## 16. Testing Summary

| Area | Test Commands | Purpose |
|---|---|---|
| Processes | `ps`, `run`, `yield` | Process information and scheduling |
| Threads | `threadtest` | Thread scheduling |
| Synchronisation | `racetest` | Race condition and mutex protection |
| Semaphores | `pctest` | Producer-consumer synchronisation |
| Memory | `meminfo`, `memtest` | Memory information and frame allocation |
| File System | `ls`, `touch`, `write`, `cat`, `rm` | File-system operations |

---

## 17. Git Milestones

Development progress is tracked using Git milestone tags:

```text
v0.1-stage0
v0.2-stage1
v0.3-stage2
v0.4-stage3
v0.5-stage4
```

These tags represent the major development stages of the operating-system project.

---

## 18. Development Environment

The project was developed and tested using:

- Windows 11
- WSL2
- Ubuntu
- NASM
- GCC
- GNU Binutils
- GNU Make
- QEMU
- Git
- GitHub

---

## 19. Important Notes

This project is a **freestanding educational kernel**. It does not depend on the normal C standard library used by regular desktop applications.

The Stage 4 file system uses RAM as its storage area. Its purpose is to demonstrate basic file-system concepts inside the kernel.

The implementation focuses on making important operating-system concepts visible and testable through a simple command-line shell.

---

## 20. Learning Outcomes

By completing the project, the following operating-system concepts are demonstrated:

- Booting an x86 system
- Protected mode
- Kernel input and output
- Processes and scheduling
- Threads
- Race conditions
- Mutexes
- Semaphores
- Physical memory management
- RAM-disk storage
- Inode-based file representation
- Basic file operations

Together, these components form a small but functional educational operating system.
