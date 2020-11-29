//
// Created by thomasf on 2020/10/29.
//

#include "urandom_writer.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

const char kUrandomPath[] = "/dev/urandom";

typedef struct {
    void *begin_addr;
    size_t size;
} write_threads_params_t;


static void *fill_memory_p(void *data) {
    if (!data) {
        fprintf(stderr, "Wrong args in `fill_memory_p`: address: 0x%lx. Aborting.\n", (u_int64_t) data);
        pthread_exit((void *) 1);
    }
    __auto_type params = (write_threads_params_t *) data;
    __auto_type handler = fopen(kUrandomPath, "rb"); // открытие файла /dev/urandom

    if (!handler) {
        printf("Couldn't open /dev/urandom. Aborting\n");
        goto aborting;
    }

    __auto_type read_sz = fread(params->begin_addr, sizeof(char), params->size, handler); // чтение из файла

    if (read_sz != params->size) {
        fprintf(stderr, "Error on reading %li bytes from /dev/urandom. Aborting\n", params->size);
        goto aborting;
    }

    fclose(handler); // закрытие файла
    pthread_exit(0);

    aborting:
    {
        fclose(handler);
        pthread_exit((void *) 1);
    };
}


bool fill_memory(void *address, size_t sz, size_t threads_n) {
    if (!address || !sz || !threads_n) {
        fprintf(stderr, "Wrong args in `fill_memory`: address: 0x%lx, sz: %li, threads_n: %li. Aborting.\n",
                (u_int64_t) address,
                sz, threads_n);
        return false;
    }

    __auto_type threads = (pthread_t *) malloc(sizeof(pthread_t) * threads_n); // выделение памяти под потоки
    __auto_type threads_params = (write_threads_params_t *) malloc(sizeof(write_threads_params_t) * threads_n);
    __auto_type total_size = sz;
    const size_t block_sz = total_size / threads_n;

    void *current_addr = address;
    for (size_t i = 0; i < threads_n; i++) {
        threads_params[i].size = block_sz;
        threads_params[i].begin_addr = current_addr;
        current_addr += block_sz;
    }

    threads_params[threads_n - 1].size += total_size % threads_n;

    // создание потоков
    for (size_t i = 0; i < threads_n; i++) {
        // https://man7.org/linux/man-pages/man3/pthread_create.3.html
        if (pthread_create(&threads[i], NULL, fill_memory_p, &threads_params[i])) {
            fprintf(stderr, "Couldn't create thread n: %zu", i);
            goto aborting;
        }
    }

    // ожидание закрытия потоков (джоины)
    for (size_t i = 0; i < threads_n; i++) {
        int32_t *result;
        if (pthread_join(threads[i], (void **) &result) || result) {
            fprintf(stderr, "Couldn't join thread n: %zu", i);
            goto aborting;
        }
    }

    free(threads_params);
    free(threads);
    return true;

    aborting:
    {
        free(threads_params);
        free(threads);
        return false;
    };
}