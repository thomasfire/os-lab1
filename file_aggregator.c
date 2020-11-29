//
// Created by thomasf on 2020/11/22.
//

#include "file_aggregator.h"

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int file_descriptor;
    size_t size_to_read;
    pthread_cond_t *cv;
    pthread_mutex_t *mtx;
    int64_t aggregated;
} agg_params;

static void *aggregate_data_p(void *data) {
    __auto_type params = (agg_params *) data;
    int8_t *buffer = malloc(sizeof(int8_t) * params->size_to_read);
    int64_t aggregated = 0;

    // чтение под мьютексами и только после того, как дождались сигнал
    if (!pthread_mutex_lock(params->mtx) && !pthread_cond_wait(params->cv, params->mtx)) {
        if (read(params->file_descriptor, buffer, params->size_to_read) != params->size_to_read) {
            fprintf(stderr, "Error on reading the file\n");
            goto aborting;
        }
    } else {
        fprintf(stderr, "Error on mutex\n");
        goto aborting;
    }

    pthread_mutex_unlock(
            params->mtx); // смысла хендлить ошибки нету, так как если они есть, то это гг и ничего не спасти
    pthread_cond_signal(params->cv);

    for (size_t i = 0; i < params->size_to_read; i++)  // аггрегация
        aggregated += buffer[i];


    params->aggregated = aggregated;

    free(buffer);
    pthread_exit(0);

    aborting:
    {
        free(buffer);
        pthread_exit((void *) 1);
    }
}

int64_t aggregate_data(size_t threads_n, filelist files) {
    int64_t agg_result = 0;
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    agg_params *params = calloc(sizeof(agg_params), threads_n);
    pthread_t *threads = calloc(sizeof(pthread_t), threads_n);

    const size_t base_threads_per_file = threads_n / files.count;
    const size_t base_block_size = files.sizes / base_threads_per_file;
    const size_t base_last_block_size = base_block_size + files.sizes % base_threads_per_file;
    const size_t last_threads_per_file = base_threads_per_file + threads_n % files.count;
    const size_t last_block_size = files.sizes / last_threads_per_file;
    const size_t last_last_block_size = last_block_size + files.sizes % last_threads_per_file;

    for (size_t i = 0; i < files.count - 1; i++) {
        int file_d = open(files.filenames[i], O_RDONLY);
        if (file_d < 0) {
            fprintf(stderr, "Error on opening file for read. Aborting...\n");
            goto aborting;
        }
        for (size_t t = 0; t < base_threads_per_file; t++) {
            params[i * base_threads_per_file + t].file_descriptor = file_d;
            params[i * base_threads_per_file + t].size_to_read = base_block_size;
            params[i * base_threads_per_file + t].cv = &cv;
            params[i * base_threads_per_file + t].mtx = &mtx;
        }
        params[(i + 1) * base_threads_per_file - 1].size_to_read = base_last_block_size;
    }

    { // last batch of threads
        const size_t t_offset = (files.count - 1) * base_threads_per_file;
        int file_d = open(files.filenames[files.count - 1], O_RDONLY);
        if (file_d < 0) {
            fprintf(stderr, "Error on opening file for read. Aborting...\n");
            goto aborting;
        }
        for (size_t t = 0; t < last_threads_per_file; t++) {
            params[t_offset + t].file_descriptor = file_d;
            params[t_offset + t].size_to_read = last_block_size;
            params[t_offset + t].cv = &cv;
            params[t_offset + t].mtx = &mtx;
        }
        params[t_offset + last_threads_per_file - 1].size_to_read = last_last_block_size;
    }

    // создание потоков
    for (size_t i = 0; i < threads_n; i++) {
        // https://man7.org/linux/man-pages/man3/pthread_create.3.html
        if (pthread_create(&threads[i], NULL, aggregate_data_p, (params + i))) {
            fprintf(stderr, "Couldn't create thread n: %zu\n", i);
            goto aborting;
        }
    }

    pthread_cond_signal(&cv); // кто-то первый ждет сигнал

    // ожидание закрытия потоков (джоины)
    for (size_t i = 0; i < threads_n; i++) {
        int32_t *result;
        if (pthread_join(threads[i], (void **) &result) || result) {
            fprintf(stderr, "Couldn't join thread n: %zu\n", i);
            goto aborting;
        }
        agg_result += params[i].aggregated;
    }

    // for Didenko - avg
    // agg_result = agg_result / (files.count * files.sizes);

    goto aborting;
    aborting:
    {
        for (int i = 0; i < threads_n; i++) {
            close(params[i].file_descriptor);
        }
        free(threads);
        free(params);
        return agg_result;
    }
}