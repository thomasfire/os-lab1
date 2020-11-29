cmake_minimum_required(VERSION 3.17)
project(oslab1 C)

set(CMAKE_C_STANDARD 11)

add_executable(oslab1
        main.c
        urandom_writer.c
        urandom_writer.h
        disk_writer.c
        disk_writer.h
        file_aggregator.c
        file_aggregator.h
)

target_link_libraries(oslab1 pthread)