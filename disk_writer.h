//
// Created by thomasf on 2020/11/01.
//

#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char **filenames;
    size_t count;
    size_t sizes; // they are same for all entries
} filelist;

/// Returns list of files
filelist write_on_disk(const void *buffer, size_t total_sz, size_t block_sz, size_t file_sz);

void free_filelist(filelist *filenames);

/// True on success
bool delete_files_if_exist(filelist *list);