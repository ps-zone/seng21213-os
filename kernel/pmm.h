#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PMM_FRAME_SIZE 4096

/*
 * Initialise the physical memory manager using
 * the BIOS E820 memory map stored by the bootloader.
 */
void pmm_init(void);

/*
 * Allocate one 4 KB physical frame.
 * Returns the physical address of the frame.
 * Returns 0 if no free frame is available.
 */
uint32_t pmm_alloc_frame(void);

/*
 * Release a previously allocated physical frame.
 */
void pmm_free_frame(uint32_t paddr);

/*
 * Memory information used by the meminfo command.
 */
uint32_t pmm_total_memory(void);
uint32_t pmm_used_memory(void);
uint32_t pmm_free_memory(void);

#endif
