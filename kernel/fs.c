#include "fs.h"
#include "ramdisk.h"

#define FS_MAGIC             0x53454E47
#define FS_DATA_START_BLOCK  2

/*
 * Simple filesystem metadata.
 */
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
} superblock_t;

/*
 * Open-file entry.
 */
typedef struct {
    uint32_t used;
    uint32_t inode_index;
    uint32_t position;
} open_file_t;

static superblock_t superblock;

static uint8_t block_bitmap[RAMDISK_BLOCKS];
static uint8_t inode_bitmap[FS_MAX_FILES];

static inode_t inodes[FS_MAX_FILES];
static dir_entry_t directory[FS_MAX_FILES];

static open_file_t open_files[FS_MAX_OPEN_FILES];


/* =========================================================
 * Small string helpers
 * ========================================================= */

static uint32_t fs_strlen(const char *str) {
    uint32_t length = 0;

    if (str == 0) {
        return 0;
    }

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


/* =========================================================
 * Find file
 * ========================================================= */

static int find_file(const char *name) {
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (directory[i].used &&
            fs_strcmp(directory[i].name, name) == 0) {
            return (int)i;
        }
    }

    return -1;
}


/* =========================================================
 * Block allocation
 * ========================================================= */

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


/* =========================================================
 * Inode allocation
 * ========================================================= */

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


/* =========================================================
 * Initialise filesystem
 * ========================================================= */

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
     * Block 0 = filesystem metadata
     * Block 1 = flat directory
     */
    block_bitmap[0] = 1;
    block_bitmap[1] = 1;

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

    for (uint32_t i = 0;
         i < FS_MAX_OPEN_FILES;
         i++) {
        open_files[i].used = 0;
        open_files[i].inode_index = 0;
        open_files[i].position = 0;
    }
}


/* =========================================================
 * Create file
 * ========================================================= */

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

    fs_strcpy(directory[directory_index].name,
              name,
              FS_MAX_FILENAME);

    directory[directory_index].inode_index =
        (uint32_t)inode_index;

    return 0;
}


/* =========================================================
 * Open / close
 * ========================================================= */

int fs_open(const char *name) {
    int directory_index = find_file(name);

    if (directory_index < 0) {
        return -1;
    }

    for (uint32_t fd = 0;
         fd < FS_MAX_OPEN_FILES;
         fd++) {

        if (!open_files[fd].used) {
            open_files[fd].used = 1;
            open_files[fd].inode_index =
                directory[directory_index].inode_index;
            open_files[fd].position = 0;

            return (int)fd;
        }
    }

    return -1;
}


int fs_close(int fd) {
    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES ||
        !open_files[fd].used) {
        return -1;
    }

    open_files[fd].used = 0;
    open_files[fd].inode_index = 0;
    open_files[fd].position = 0;

    return 0;
}


/* =========================================================
 * Descriptor-based read
 * ========================================================= */

int fs_read_fd(int fd,
               char *buffer,
               uint32_t count) {
    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES ||
        !open_files[fd].used ||
        buffer == 0) {
        return -1;
    }

    inode_t *inode =
        &inodes[open_files[fd].inode_index];

    uint32_t position = open_files[fd].position;

    if (position >= inode->size) {
        return 0;
    }

    uint32_t remaining = inode->size - position;

    if (count > remaining) {
        count = remaining;
    }

    uint32_t copied = 0;

    while (copied < count) {
        uint32_t file_position = position + copied;
        uint32_t block_index =
            file_position / FS_BLOCK_SIZE;
        uint32_t block_offset =
            file_position % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS ||
            inode->blocks[block_index] == 0) {
            break;
        }

        uint8_t block_buffer[FS_BLOCK_SIZE];

        if (ramdisk_read_block(
                inode->blocks[block_index],
                block_buffer) != 0) {
            return -1;
        }

        while (block_offset < FS_BLOCK_SIZE &&
               copied < count) {
            buffer[copied] =
                (char)block_buffer[block_offset];

            copied++;
            block_offset++;
        }
    }

    open_files[fd].position += copied;

    return (int)copied;
}


/* =========================================================
 * Descriptor-based write
 *
 * Writes at the current file position.
 * ========================================================= */

int fs_write_fd(int fd,
                const char *buffer,
                uint32_t count) {
    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES ||
        !open_files[fd].used ||
        buffer == 0) {
        return -1;
    }

    inode_t *inode =
        &inodes[open_files[fd].inode_index];

    uint32_t position = open_files[fd].position;

    if (position + count >
        FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) {
        return -1;
    }

    uint32_t written = 0;

    while (written < count) {
        uint32_t file_position = position + written;
        uint32_t block_index =
            file_position / FS_BLOCK_SIZE;
        uint32_t block_offset =
            file_position % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            break;
        }

        if (inode->blocks[block_index] == 0) {
            int new_block = allocate_block();

            if (new_block < 0) {
                return written > 0 ?
                    (int)written : -1;
            }

            inode->blocks[block_index] =
                (uint32_t)new_block;

            uint8_t empty_block[FS_BLOCK_SIZE];

            for (uint32_t i = 0;
                 i < FS_BLOCK_SIZE;
                 i++) {
                empty_block[i] = 0;
            }

            if (ramdisk_write_block(
                    (uint32_t)new_block,
                    empty_block) != 0) {
                free_block((uint32_t)new_block);
                inode->blocks[block_index] = 0;
                return -1;
            }
        }

        uint8_t block_buffer[FS_BLOCK_SIZE];

        if (ramdisk_read_block(
                inode->blocks[block_index],
                block_buffer) != 0) {
            return -1;
        }

        while (block_offset < FS_BLOCK_SIZE &&
               written < count) {
            block_buffer[block_offset] =
                (uint8_t)buffer[written];

            written++;
            block_offset++;
        }

        if (ramdisk_write_block(
                inode->blocks[block_index],
                block_buffer) != 0) {
            return -1;
        }
    }

    open_files[fd].position += written;

    if (open_files[fd].position > inode->size) {
        inode->size = open_files[fd].position;
    }

    return (int)written;
}


/* =========================================================
 * Delete / unlink
 * ========================================================= */

int fs_unlink(const char *name) {
    int directory_index = find_file(name);

    if (directory_index < 0) {
        return -1;
    }

    uint32_t inode_index =
        directory[directory_index].inode_index;

    /*
     * Do not unlink an open file.
     */
    for (uint32_t fd = 0;
         fd < FS_MAX_OPEN_FILES;
         fd++) {

        if (open_files[fd].used &&
            open_files[fd].inode_index ==
                inode_index) {
            return -1;
        }
    }

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


int fs_delete(const char *name) {
    return fs_unlink(name);
}


/* =========================================================
 * Shell-friendly write
 *
 * The shell command appends text to the file.
 * ========================================================= */

int fs_write(const char *name, const char *data) {
    if (name == 0 || data == 0) {
        return -1;
    }

    int directory_index = find_file(name);

    if (directory_index < 0) {
        return -1;
    }

    int fd = fs_open(name);

    if (fd < 0) {
        return -1;
    }

    inode_t *inode =
        &inodes[directory[directory_index].inode_index];

    /*
     * Move the descriptor to the end so shell writes append.
     */
    open_files[fd].position = inode->size;

    int result =
        fs_write_fd(fd, data, fs_strlen(data));

    fs_close(fd);

    if (result < 0) {
        return -1;
    }

    return 0;
}


/* =========================================================
 * Shell-friendly read
 * ========================================================= */

int fs_read(const char *name,
            char *buffer,
            uint32_t buffer_size) {
    if (name == 0 ||
        buffer == 0 ||
        buffer_size == 0) {
        return -1;
    }

    int fd = fs_open(name);

    if (fd < 0) {
        return -1;
    }

    int result =
        fs_read_fd(fd,
                   buffer,
                   buffer_size - 1);

    if (result < 0) {
        fs_close(fd);
        return -1;
    }

    buffer[result] = '\0';

    fs_close(fd);

    return result;
}


/* =========================================================
 * Directory information
 * ========================================================= */

uint32_t fs_file_count(void) {
    uint32_t count = 0;

    for (uint32_t i = 0;
         i < FS_MAX_FILES;
         i++) {

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
