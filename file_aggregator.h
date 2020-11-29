//
// Created by thomasf on 2020/11/22.
//

#pragma once

#include "disk_writer.h"

#include <stdint.h>

int64_t aggregate_data(size_t threads_n, filelist files);