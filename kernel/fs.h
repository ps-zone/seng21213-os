#ifndef FS_H
#define FS_H

#include "../include/types.h"

#define FS_MAX_FILES          32
#define FS_MAX_FILENAME       32
#define FS_DIRECT_BLOCKS      8
#define FS_BLOCK_SIZE         4096

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

/* Create an empty file. */
int fs_create(const char *name);

/* Delete a file. */
int fs_delete(const char *name);

/* Write data to a file. */
int fs_write(const char *name, const char *data);

/*
 * Read a file.
 * Returns number of bytes read, or -1 on error.
 */
int fs_read(const char *name, char *buffer, uint32_t buffer_size);

/* File/directory information for shell commands. */
uint32_t fs_file_count(void);
const dir_entry_t *fs_get_directory_entry(uint32_t index);

#endif
