#include "pmm.h"

/*
 * BIOS E820 memory map locations.
 * These must match the addresses used by boot/boot.asm.
 */
#define E820_COUNT_ADDRESS 0x5000
#define E820_MAP_ADDRESS   0x5004

#define E820_USABLE        1

/*
 * Support up to 4 GB of physical memory.
 *
 * 4 GB / 4 KB = 1,048,576 frames
 * 1 bit per frame = 131,072 bytes bitmap.
 */
#define PMM_MAX_FRAMES     1048576
#define PMM_BITMAP_SIZE    (PMM_MAX_FRAMES / 8)

/*
 * BIOS E820 entry.
 *
 * base   = starting physical address
 * length = size of the region
 * type 1 = usable RAM
 */
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} e820_entry_t;

static uint8_t frame_bitmap[PMM_BITMAP_SIZE];

static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

/* Mark a frame as used. */
static void bitmap_set(uint32_t frame) {
    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    frame_bitmap[frame / 8] |=
        (uint8_t)(1U << (frame % 8));
}

/* Mark a frame as free. */
static void bitmap_clear(uint32_t frame) {
    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    frame_bitmap[frame / 8] &=
        (uint8_t)~(1U << (frame % 8));
}

/* Check whether a frame is currently used. */
static uint32_t bitmap_test(uint32_t frame) {
    if (frame >= PMM_MAX_FRAMES) {
        return 1;
    }

    return frame_bitmap[frame / 8] &
           (uint8_t)(1U << (frame % 8));
}

void pmm_init(void) {
    /*
     * Begin with every physical frame marked as used.
     * E820 usable regions will then be marked free.
     */
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        frame_bitmap[i] = 0xFF;
    }

    total_frames = 0;
    used_frames = 0;

    volatile uint16_t *entry_count =
        (volatile uint16_t *)E820_COUNT_ADDRESS;

    volatile e820_entry_t *entries =
        (volatile e820_entry_t *)E820_MAP_ADDRESS;

    uint16_t count = *entry_count;

    for (uint16_t i = 0; i < count; i++) {

        if (entries[i].type != E820_USABLE) {
            continue;
        }

        uint64_t start = entries[i].base;
        uint64_t end =
            entries[i].base + entries[i].length;

        /*
         * Ignore memory above 4 GB because this is
         * currently a 32-bit kernel.
         */
        if (start >= 0x100000000ULL) {
            continue;
        }

        if (end > 0x100000000ULL) {
            end = 0x100000000ULL;
        }

        /*
         * Only completely usable 4 KB frames are freed.
         */
        uint64_t first_frame =
            (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

        uint64_t last_frame =
            end / PMM_FRAME_SIZE;

        for (uint64_t frame = first_frame;
             frame < last_frame;
             frame++) {

            if (frame < PMM_MAX_FRAMES) {
                bitmap_clear((uint32_t)frame);
                total_frames++;
            }
        }
    }

    /*
     * Reserve low physical memory below 1 MB.
     *
     * This protects the bootloader, E820 map,
     * kernel loading area, BIOS data and kernel stack area.
     */
    uint32_t reserved_frames =
        0x100000 / PMM_FRAME_SIZE;

    for (uint32_t frame = 0;
         frame < reserved_frames;
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);

            if (total_frames > 0) {
                total_frames--;
            }
        }
    }

  /*
   * Reserve the physical frame directly below 2 MB
   * for the main kernel stack.
   *
   * The bootloader starts ESP at 0x200000, so the stack
   * grows downward into this 4 KB frame.
   */
  uint32_t kernel_stack_frame =
      (0x00200000 / PMM_FRAME_SIZE) - 1;

  if (!bitmap_test(kernel_stack_frame)) {
      bitmap_set(kernel_stack_frame);

    if (total_frames > 0) {
        total_frames--;
      }
  }

    /*
     * At this point total_frames represents usable,
     * allocatable physical frames.
     */
    used_frames = 0;
}

uint32_t pmm_alloc_frame(void) {

    /*
     * First-fit scan required by Stage 3.
     */
    for (uint32_t frame = 0;
         frame < PMM_MAX_FRAMES;
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            used_frames++;

            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t paddr) {

    if ((paddr % PMM_FRAME_SIZE) != 0) {
        return;
    }

    uint32_t frame = paddr / PMM_FRAME_SIZE;

    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    if (bitmap_test(frame)) {
        bitmap_clear(frame);

        if (used_frames > 0) {
            used_frames--;
        }
    }
}

uint32_t pmm_total_memory(void) {
    return total_frames * PMM_FRAME_SIZE;
}

uint32_t pmm_used_memory(void) {
    return used_frames * PMM_FRAME_SIZE;
}

uint32_t pmm_free_memory(void) {
    return (total_frames - used_frames) *
           PMM_FRAME_SIZE;
}
