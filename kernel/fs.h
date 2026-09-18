#ifndef FS_H
#define FS_H

#include "../include/types.h"

#define FS_MAX_FILES          32
#define FS_MAX_FILENAME       28
#define FS_DIRECT_BLOCKS      8
#define FS_BLOCK_SIZE         4096
#define FS_MAX_OPEN_FILES     16

/*
 * Each inode represents one file.
 * A file can use up to 8 direct 4 KB blocks.
 */
typedef struct {
    uint32_t used;
    uint32_t size;
    uint32_t blocks[FS_DIRECT_BLOCKS];
} inode_t;

/*
 * Flat-directory entry.
 * Maps a filename to an inode.
 */
typedef struct {
    uint32_t used;
    char name[FS_MAX_FILENAME];
    uint32_t inode_index;
} dir_entry_t;

/* Initialise and format the RAM-disk filesystem. */
void fs_init(void);

/*
 * POSIX-inspired file interface.
 */
int fs_open(const char *name);
int fs_close(int fd);
int fs_read(int fd, char *buffer, uint32_t count);
int fs_write(int fd, const char *buffer, uint32_t count);
int fs_unlink(const char *name);

/*
 * Convenience functions used by the kernel shell.
 */
int fs_create(const char *name);
int fs_delete(const char *name);
int fs_write_file(const char *name, const char *data);
int fs_read_file(const char *name, char *buffer, uint32_t buffer_size);

/* Directory information used by ls. */
uint32_t fs_file_count(void);
const dir_entry_t *fs_get_directory_entry(uint32_t index);

#endif
