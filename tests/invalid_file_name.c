#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *file_path = "/f11111111111111111111111111111111111111";

    tfs_params params = tfs_default_params();
    params.max_inode_count = 3;
    params.max_block_count = 3;
    assert(tfs_init(&params) != -1);

    assert(tfs_open(file_path, TFS_O_CREAT) == -1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
}