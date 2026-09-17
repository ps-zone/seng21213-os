#include "ramdisk.h"

/*
 * Stage 4 RAM Disk
 *
 * The RAM disk is a fixed-size 1 MB byte array
 * stored in the kernel BSS.
 *
 * Size       : 1 MB
 * Block size : 4 KB
 * Blocks     : 256
 */

/*
 * 1 MB Stage 4 RAM disk stored in the kernel BSS.
 */
static uint8_t ramdisk[RAMDISK_SIZE];


/*
 * Initialise the RAM disk by clearing all 1 MB.
 */
void ramdisk_init(void) {

    for (uint32_t i = 0; i < RAMDISK_SIZE; i++) {
        ramdisk[i] = 0;
    }
}


/*
 * Read one 4 KB block.
 */
int ramdisk_read_block(uint32_t block, void *buffer) {

    if (block >= RAMDISK_BLOCKS || buffer == 0) {
        return -1;
    }

    uint8_t *destination =
        (uint8_t *)buffer;

    uint32_t offset =
        block * RAMDISK_BLOCK_SIZE;

    for (uint32_t i = 0;
         i < RAMDISK_BLOCK_SIZE;
         i++) {

        destination[i] =
            ramdisk[offset + i];
    }

    return 0;
}


/*
 * Write one 4 KB block.
 */
int ramdisk_write_block(uint32_t block,
                        const void *buffer) {

    if (block >= RAMDISK_BLOCKS || buffer == 0) {
        return -1;
    }

    const uint8_t *source =
        (const uint8_t *)buffer;

    uint32_t offset =
        block * RAMDISK_BLOCK_SIZE;

    for (uint32_t i = 0;
         i < RAMDISK_BLOCK_SIZE;
         i++) {

        ramdisk[offset + i] =
            source[i];
    }

    return 0;
}


/*
 * Return total number of blocks.
 */
uint32_t ramdisk_block_count(void) {
    return RAMDISK_BLOCKS;
}
