#include "fs.h"
#include "ramdisk.h"

#define FS_MAGIC             0x53454E47
#define FS_DATA_START_BLOCK  1

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
} superblock_t;

static superblock_t superblock;

static uint8_t block_bitmap[RAMDISK_BLOCKS];
static uint8_t inode_bitmap[FS_MAX_FILES];

static inode_t inodes[FS_MAX_FILES];
static dir_entry_t directory[FS_MAX_FILES];


/* ---------------------------------------------------------
 * Small internal string helpers
 * --------------------------------------------------------- */

static uint32_t fs_strlen(const char *str) {
    uint32_t length = 0;

    while (str[length] != '\0') {
        length++;
    }

    return length;
}

static int fs_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }

    return (int)((unsigned char)*a -
                 (unsigned char)*b);
}

static void fs_strcpy(char *dest,
                      const char *src,
                      uint32_t max_length) {

    uint32_t i = 0;

    while (src[i] != '\0' &&
           i < max_length - 1) {

        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}


/* ---------------------------------------------------------
 * Find a directory entry by filename
 * --------------------------------------------------------- */

static int find_file(const char *name) {

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        if (directory[i].used &&
            fs_strcmp(directory[i].name, name) == 0) {

            return (int)i;
        }
    }

    return -1;
}


/* ---------------------------------------------------------
 * Block allocation
 * --------------------------------------------------------- */

static int allocate_block(void) {

    for (uint32_t block = FS_DATA_START_BLOCK;
         block < RAMDISK_BLOCKS;
         block++) {

        if (!block_bitmap[block]) {

            block_bitmap[block] = 1;

            if (superblock.free_blocks > 0) {
                superblock.free_blocks--;
            }

            return (int)block;
        }
    }

    return -1;
}

static void free_block(uint32_t block) {

    if (block < FS_DATA_START_BLOCK ||
        block >= RAMDISK_BLOCKS) {
        return;
    }

    if (block_bitmap[block]) {
        block_bitmap[block] = 0;
        superblock.free_blocks++;
    }
}


/* ---------------------------------------------------------
 * Inode allocation
 * --------------------------------------------------------- */

static int allocate_inode(void) {

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        if (!inode_bitmap[i]) {

            inode_bitmap[i] = 1;

            if (superblock.free_inodes > 0) {
                superblock.free_inodes--;
            }

            return (int)i;
        }
    }

    return -1;
}

static void free_inode(uint32_t inode_index) {

    if (inode_index >= FS_MAX_FILES) {
        return;
    }

    if (inode_bitmap[inode_index]) {
        inode_bitmap[inode_index] = 0;
        superblock.free_inodes++;
    }
}


/* ---------------------------------------------------------
 * Initialise filesystem
 * --------------------------------------------------------- */

void fs_init(void) {

    ramdisk_init();

    superblock.magic = FS_MAGIC;
    superblock.total_blocks = RAMDISK_BLOCKS;
    superblock.free_blocks =
        RAMDISK_BLOCKS - FS_DATA_START_BLOCK;

    superblock.total_inodes = FS_MAX_FILES;
    superblock.free_inodes = FS_MAX_FILES;

    for (uint32_t i = 0; i < RAMDISK_BLOCKS; i++) {
        block_bitmap[i] = 0;
    }

    /*
     * Block 0 is reserved for filesystem metadata.
     */
    block_bitmap[0] = 1;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        inode_bitmap[i] = 0;

        inodes[i].used = 0;
        inodes[i].size = 0;

        for (uint32_t j = 0;
             j < FS_DIRECT_BLOCKS;
             j++) {

            inodes[i].blocks[j] = 0;
        }

        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].inode_index = 0;
    }
}


/* ---------------------------------------------------------
 * Create file
 * --------------------------------------------------------- */

int fs_create(const char *name) {

    if (name == 0 ||
        name[0] == '\0' ||
        fs_strlen(name) >= FS_MAX_FILENAME) {

        return -1;
    }

    if (find_file(name) >= 0) {
        return -1;
    }

    int inode_index = allocate_inode();

    if (inode_index < 0) {
        return -1;
    }

    int directory_index = -1;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        if (!directory[i].used) {
            directory_index = (int)i;
            break;
        }
    }

    if (directory_index < 0) {
        free_inode((uint32_t)inode_index);
        return -1;
    }

    inode_t *inode = &inodes[inode_index];

    inode->used = 1;
    inode->size = 0;

    for (uint32_t i = 0;
         i < FS_DIRECT_BLOCKS;
         i++) {

        inode->blocks[i] = 0;
    }

    directory[directory_index].used = 1;

    fs_strcpy(
        directory[directory_index].name,
        name,
        FS_MAX_FILENAME
    );

    directory[directory_index].inode_index =
        (uint32_t)inode_index;

    return 0;
}


/* ---------------------------------------------------------
 * Delete file
 * --------------------------------------------------------- */

int fs_delete(const char *name) {

    int directory_index = find_file(name);

    if (directory_index < 0) {
        return -1;
    }

    uint32_t inode_index =
        directory[directory_index].inode_index;

    inode_t *inode = &inodes[inode_index];

    for (uint32_t i = 0;
         i < FS_DIRECT_BLOCKS;
         i++) {

        if (inode->blocks[i] != 0) {
            free_block(inode->blocks[i]);
            inode->blocks[i] = 0;
        }
    }

    inode->used = 0;
    inode->size = 0;

    free_inode(inode_index);

    directory[directory_index].used = 0;
    directory[directory_index].name[0] = '\0';
    directory[directory_index].inode_index = 0;

    return 0;
}


/* ---------------------------------------------------------
 * Write file
 * --------------------------------------------------------- */

int fs_write(const char *name, const char *data) {

    int directory_index = find_file(name);

    if (directory_index < 0 || data == 0) {
        return -1;
    }

    uint32_t inode_index =
        directory[directory_index].inode_index;

    inode_t *inode = &inodes[inode_index];

    /*
     * Release any blocks from previous contents.
     */
    for (uint32_t i = 0;
         i < FS_DIRECT_BLOCKS;
         i++) {

        if (inode->blocks[i] != 0) {
            free_block(inode->blocks[i]);
            inode->blocks[i] = 0;
        }
    }

    inode->size = 0;

    uint32_t length = fs_strlen(data);

    uint32_t maximum_size =
        FS_DIRECT_BLOCKS * FS_BLOCK_SIZE;

    if (length > maximum_size) {
        return -1;
    }

    if (length == 0) {
        return 0;
    }

    uint32_t blocks_needed =
        (length + FS_BLOCK_SIZE - 1) /
        FS_BLOCK_SIZE;

    uint32_t position = 0;

    for (uint32_t i = 0;
         i < blocks_needed;
         i++) {

        int block = allocate_block();

        if (block < 0) {

            /*
             * Roll back blocks already allocated.
             */
            for (uint32_t j = 0; j < i; j++) {

                free_block(inode->blocks[j]);
                inode->blocks[j] = 0;
            }

            inode->size = 0;

            return -1;
        }

        inode->blocks[i] = (uint32_t)block;

        uint8_t block_buffer[FS_BLOCK_SIZE];

        for (uint32_t j = 0;
             j < FS_BLOCK_SIZE;
             j++) {

            block_buffer[j] = 0;
        }

        for (uint32_t j = 0;
             j < FS_BLOCK_SIZE &&
             position < length;
             j++) {

            block_buffer[j] =
                (uint8_t)data[position];

            position++;
        }

        if (ramdisk_write_block(
                (uint32_t)block,
                block_buffer) != 0) {

            return -1;
        }
    }

    inode->size = length;

    return 0;
}


/* ---------------------------------------------------------
 * Read file
 * --------------------------------------------------------- */

int fs_read(const char *name,
            char *buffer,
            uint32_t buffer_size) {

    int directory_index = find_file(name);

    if (directory_index < 0 ||
        buffer == 0 ||
        buffer_size == 0) {

        return -1;
    }

    uint32_t inode_index =
        directory[directory_index].inode_index;

    inode_t *inode = &inodes[inode_index];

    uint32_t bytes_to_read = inode->size;

    /*
     * Leave room for the terminating NUL character.
     */
    if (bytes_to_read >= buffer_size) {
        bytes_to_read = buffer_size - 1;
    }

    uint32_t position = 0;

    for (uint32_t i = 0;
         i < FS_DIRECT_BLOCKS &&
         position < bytes_to_read;
         i++) {

        if (inode->blocks[i] == 0) {
            break;
        }

        uint8_t block_buffer[FS_BLOCK_SIZE];

        if (ramdisk_read_block(
                inode->blocks[i],
                block_buffer) != 0) {

            return -1;
        }

        for (uint32_t j = 0;
             j < FS_BLOCK_SIZE &&
             position < bytes_to_read;
             j++) {

            buffer[position] =
                (char)block_buffer[j];

            position++;
        }
    }

    buffer[position] = '\0';

    return (int)position;
}


/* ---------------------------------------------------------
 * Directory information
 * --------------------------------------------------------- */

uint32_t fs_file_count(void) {

    uint32_t count = 0;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {

        if (directory[i].used) {
            count++;
        }
    }

    return count;
}

const dir_entry_t *
fs_get_directory_entry(uint32_t index) {

    if (index >= FS_MAX_FILES) {
        return 0;
    }

    return &directory[index];
}
