#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

void *thread_func() {
    const char *file_path = "/f1";
    const char *link_path = "/l1";

    assert(tfs_link(file_path, link_path) == -1);

    return NULL;
}

int main() {

    pthread_t tid[3];

    tfs_params params = tfs_default_params();
    params.max_inode_count = 3;
    params.max_block_count = 3;
    assert(tfs_init(&params) != -1);

    pthread_create(&tid[0], NULL, thread_func, NULL);
    pthread_create(&tid[1], NULL, thread_func, NULL);
    pthread_create(&tid[2], NULL, thread_func, NULL);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
}