#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

void *thread_func() {

    char *str_ext_file =
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! BBB! "
        "BBB! BBB! BBB! BBB! BBB! ";
    char *path_copied_file = "/f1";
    char *path_src = "tests/file_to_copy_over512.txt";
    char buffer[600];

    int f;
    ssize_t r;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    return NULL;
}

int main() {

    pthread_t tid[3];

    assert(tfs_init(NULL) != -1);

    pthread_create(&tid[0], NULL, thread_func, NULL);
    pthread_create(&tid[1], NULL, thread_func, NULL);
    pthread_create(&tid[2], NULL, thread_func, NULL);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
