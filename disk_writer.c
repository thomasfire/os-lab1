//
// Created by thomasf on 2020/11/01.
//

#include "disk_writer.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdint.h>

static size_t file_size;
static size_t block_size;
const void *buffer_;
uint16_t waiters = 0;

typedef struct {
    const char *filename;
    size_t number;
    pthread_cond_t *cv;
    pthread_mutex_t *mtx;
} write_params;

static void *write_on_disk_p(void *data) {
    __auto_type params = (write_params *) data;
    const char *filename = params->filename;
    __auto_type num = params->number;

    __auto_type file_d = creat(filename, O_CREAT); // создание файла
    if (chmod(filename, // сеттинг прав на файлы
              S_IWUSR | S_IRUSR)) { // https://www.gnu.org/software/libc/manual/html_node/Setting-Permissions.html
        fprintf(stderr, "Error on setting file permissions, aborting...\n");
    }

    waiters++;
    if (!pthread_mutex_lock(params->mtx) && !pthread_cond_wait(params->cv, params->mtx)) {
        size_t j = 0;
        while (1) {
            if (j >= file_size)
                break;
            if (write(file_d, &buffer_[num * j], block_size) != block_size) { // запись блоков памяти в файлы
                fprintf(stderr, "Error on writing to the files. Aborting\n");
                break;
            }
            j += block_size;
        }
    }

    close(file_d);
    pthread_mutex_unlock(
            params->mtx); // смысла хендлить ошибки нету, так как если они есть, то это гг и ничего не спасти
    pthread_cond_signal(params->cv);

    pthread_exit(0);
}

filelist write_on_disk(const void *buffer, size_t total_sz, size_t block_sz, size_t file_sz) {
    const size_t files_n = total_sz / file_sz;
    filelist filels;
    filels.filenames = (char **) calloc(sizeof(char *), files_n);
    filels.count = files_n;
    filels.sizes = file_sz;
    __auto_type params = (write_params *) malloc(sizeof(write_params) * files_n);
    pthread_t *threads = calloc(sizeof(pthread_t), files_n);
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    block_size = block_sz;
    buffer_ = buffer;
    file_size = file_sz;

    if (!filels.filenames) {
        fprintf(stderr, "Error on calloc at creating list of filenames. Aborting\n");
        goto aborting;
    }

    for (size_t i = 0; i < files_n; i++) {
        filels.filenames[i] = (char *) calloc(sizeof(char), 20);
        if (!filels.filenames[i]) {
            fprintf(stderr, "Error on calloc at creating list of filenames. Aborting\n");
            goto aborting;
        }

        if (sprintf(filels.filenames[i], "output_%zu", i) < 0) {
            fprintf(stderr, "Error on sprintf at creating list of filenames. Aborting\n");
            goto aborting;
        }
        params[i].number = files_n;
        params[i].filename = filels.filenames[i];
        params[i].cv = &cv;
        params[i].mtx = &mtx;

        if (pthread_create(&threads[i], NULL, write_on_disk_p, (params + i))) {
            fprintf(stderr, "Couldn't create thread n: %zu\n", i);
            goto aborting;
        }
    }

    while (!waiters) {
        sleep(1);
    }
    pthread_cond_signal(&cv);

    for (size_t i = 0; i < files_n; i++) {
        int32_t *result;
        if (pthread_join(threads[i], (void **) &result) || result) {
            fprintf(stderr, "Couldn't join thread n: %zu\n", i);
            goto aborting;
        }
    }

    free(params);
    free(threads);
    return filels;

    aborting:
    {
        free(params);
        free(threads);
        free_filelist(&filels);
        return filels;
    };
}

void free_filelist(filelist *filenames) {
    if (!filenames || !filenames->filenames)
        return;

    for (size_t i = 0; i < filenames->count; i++)
        if (filenames->filenames[i])
            free(filenames->filenames[i]);

    free(filenames->filenames);
    filenames->filenames = NULL;
    filenames->count = 0;
}

bool delete_files_if_exist(filelist *filenames) {
    if (!filenames || !filenames->filenames)
        return false;

    for (size_t i = 0; i < filenames->count; i++)
        if (filenames->filenames[i])
            if (unlink( // удаление файлов
                    filenames->filenames[i])) // https://www.gnu.org/software/libc/manual/html_node/Deleting-Files.html
                fprintf(stderr, "Couldn't delete file `%s`, skipped.\n", filenames->filenames[i]);

    return true;
}