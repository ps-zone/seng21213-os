#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define RAMDISK_SIZE       (1024 * 1024)
#define RAMDISK_BLOCK_SIZE 4096
#define RAMDISK_BLOCKS     (RAMDISK_SIZE / RAMDISK_BLOCK_SIZE)

/*
 * Initialise the 1 MB RAM disk.
 */
void ramdisk_init(void);

/*
 * Read one 4 KB block from the RAM disk.
 * Returns 0 on success, -1 on error.
 */
int ramdisk_read_block(uint32_t block, void *buffer);

/*
 * Write one 4 KB block to the RAM disk.
 * Returns 0 on success, -1 on error.
 */
int ramdisk_write_block(uint32_t block, const void *buffer);

/*
 * Return the number of blocks in the RAM disk.
 */
uint32_t ramdisk_block_count(void);

#endif

