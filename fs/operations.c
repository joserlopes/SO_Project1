#include "operations.h"
#include "config.h"
#include "state.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "betterassert.h"

static pthread_mutex_t tfs_mutex;

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    pthread_mutex_init(&tfs_mutex, NULL);

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }

    pthread_mutex_destroy(&tfs_mutex);

    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/' &&
           strlen(name) < MAX_FILE_NAME;
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t *root_inode) {

    ALWAYS_ASSERT(root_inode->i_node_type == T_DIRECTORY,
                  "tfs_lookup: root_inode doesn't refer to the root directory");

    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;
    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {

    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");

    pthread_mutex_lock(&tfs_mutex);
    int inum = tfs_lookup(name, root_dir_inode);

    size_t offset;

    if (inum >= 0) {

        // if file already exists
        if (mode & TFS_O_STRICT_CREATE) {
            pthread_mutex_unlock(&tfs_mutex);
            return -1;
        }

        pthread_mutex_unlock(&tfs_mutex);
        inode_t *inode = inode_get(inum);

        // if the file to open is a symlink, recursively "go back" to the
        // original file
        if (inode->origin_path[0] != 0)
            return tfs_open(inode->origin_path, mode);

        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            pthread_mutex_unlock(&tfs_mutex);
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            pthread_mutex_unlock(&tfs_mutex);
            inode_delete(inum);
            return -1; // no space in directory
        }
        offset = 0;
        pthread_mutex_unlock(&tfs_mutex);
    } else {
        pthread_mutex_unlock(&tfs_mutex);
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int link_inumber = inode_create(T_FILE);

    inode_t *link_inode = inode_get(link_inumber);

    strncpy(link_inode->origin_path, target, MAX_FILE_NAME - 1);
    link_inode->origin_path[MAX_FILE_NAME - 1] = '\0';

    // target doesn't exist
    if (tfs_lookup(target, root_dir_inode) == -1)
        return -1;

    if (add_dir_entry(root_dir_inode, link_name + 1, link_inumber) != 0)
        return -1;

    return 0;
}

int tfs_link(char const *target, char const *link_name) {

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    int inumber = tfs_lookup(target, root_dir_inode);

    // target doesn't exist
    if (inumber < 0)
        return -1;

    inode_t *file_inode = inode_get(inumber);

    // hard link to sym link
    if (file_inode->origin_path[0] != '\0')
        return -1;

    // link_name + 1 to skip the initial '/' character
    if (add_dir_entry(root_dir_inode, link_name + 1, inumber) != 0)
        return -1;

    file_inode->i_hard_link_count++;
    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&file->lock);
    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    pthread_rwlock_wrlock(&inode->lock);

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                pthread_rwlock_unlock(&inode->lock);
                pthread_mutex_unlock(&file->lock);
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    pthread_rwlock_unlock(&inode->lock);
    pthread_mutex_unlock(&file->lock);

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    pthread_mutex_lock(&tfs_mutex);
    pthread_mutex_lock(&file->lock);

    // From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    pthread_rwlock_rdlock(&inode->lock);

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    pthread_rwlock_unlock(&inode->lock);
    pthread_mutex_unlock(&file->lock);
    pthread_mutex_unlock(&tfs_mutex);
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    int inumber = tfs_lookup(target, root_dir_inode);
    inode_t *file_inode = inode_get(inumber);

    if (!valid_pathname(target))
        return -1;

    if (is_in_open_file_table(inumber))
        return -1;

    // target + 1 to skip the initial '/' character
    if (clear_dir_entry(root_dir_inode, target + 1) != 0)
        return -1;

    file_inode->i_hard_link_count--;
    if (file_inode->i_hard_link_count == 0)
        inode_delete(inumber);
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {

    char buffer[BUFSIZ];
    size_t numRead;

    FILE *inputFp = fopen(source_path, "rb");
    int outputFp = tfs_open(dest_path, TFS_O_CREAT | TFS_O_TRUNC);

    if (inputFp == NULL || outputFp == -1)
        return -1;

    memset(buffer, 0, sizeof(buffer));

    while ((numRead = fread(buffer, sizeof(char), sizeof(buffer), inputFp)) >
           0) {
        if (tfs_write(outputFp, buffer, strlen(buffer)) != numRead)
            return -1;
    }

    fclose(inputFp);
    tfs_close(outputFp);

    return 0;
}
