//
// Created by thomasf on 2020/11/01.
//

#include "disk_writer.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

// достаем недостающие данные, чтобы файлы были одинакового размера
void *generate_block(size_t sz) {
    __auto_type block = (void *) malloc(sz);
    int rnd = open("/dev/urandom", O_RDONLY);
    read(rnd, block, sz);
    close(rnd);
    return block;
}

filelist write_on_disk(const void *buffer, size_t total_sz, size_t block_sz, size_t file_sz) {
    const size_t files_n = total_sz / file_sz + ((total_sz % file_sz) ? 1 : 0);
    filelist filels;
    filels.filenames = (char **) calloc(sizeof(char *), files_n);
    filels.count = files_n;
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
    }

    for (size_t i = 0; i < files_n; i++) {
        __auto_type file_d = creat(filels.filenames[i], O_CREAT); // создание файла
        if (chmod(filels.filenames[i], // сеттинг прав на файлы
                  S_IWUSR | S_IRUSR)) { // https://www.gnu.org/software/libc/manual/html_node/Setting-Permissions.html
            fprintf(stderr, "Error on setting file permissions, aborting...\n");
            goto aborting;
        }

        for (size_t j = 0; j < file_sz; j += block_sz) {
            if (write(file_d, &buffer[i * j], block_sz) != block_sz) { // запись блоков памяти в файлы
                fprintf(stderr, "Error on writing to the files. Aborting\n");
                goto aborting;
            }
        }

        close(file_d);
    }

    return filels;

    aborting:
    {
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