
#include "urandom_writer.h"
#include "disk_writer.h"
#include "file_aggregator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Savkin: A=177;B=0x1E3A66B2;C=malloc;D=10;E=151;F=block;G=25;H=seq;I=99;J=sum;K=cv
// Didenko: A=111;B=0x6FDC126E;C=malloc;D=113;E=126;F=block;G=71;H=seq;I=130;J=avg;K=cv

/*
Разработать программу на языке С, которая осуществляет следующие действия
    Создает область памяти размером A мегабайт, начинающихся с адреса B (если возможно) при помощи C=(malloc, mmap) заполненную случайными числами /dev/urandom в D потоков. Используя системные средства мониторинга определите адрес начала в адресном пространстве процесса и характеристики выделенных участков памяти. Замеры виртуальной/физической памяти необходимо снять:
        До аллокации
        После аллокации
        После заполнения участка данными
        После деаллокации
    Записывает область памяти в файлы одинакового размера E мегабайт с использованием F=(блочного, некешируемого) обращения к диску. Размер блока ввода-вывода G байт. Преподаватель выдает в качестве задания последовательность записи/чтения блоков H=(последовательный, заданный  или случайный)
    Генерацию данных и запись осуществлять в бесконечном цикле.
    В отдельных I потоках осуществлять чтение данных из файлов и подсчитывать агрегированные характеристики данных - J=(сумму, среднее значение, максимальное, минимальное значение).
    Чтение и запись данных в/из файла должна быть защищена примитивами синхронизации K=(futex, cv, sem, flock).
    По заданию преподавателя изменить приоритеты потоков и описать изменения в характеристиках программы.
Для запуска программы возможно использовать операционную систему Windows 10 или  Debian/Ubuntu в виртуальном окружении.
Измерить значения затраченного процессорного времени на выполнение программы и на операции ввода-вывода используя системные утилиты.
Отследить трассу системных вызовов.
Используя stap построить графики системных характеристик.
 * */

const size_t kMegaByteSize = 1048576;
const size_t kMallocSize = 17 * kMegaByteSize;
//const size_t kInitAddress = 0x1E3A66B2;
const size_t kWriteThreadsCount = 10;
const size_t kWriteSize = 15 * kMegaByteSize;
const size_t kBlockSize = 71;
const size_t kReadThreadsCount = 99;

// делаем остановку программы (для удобного захвата статистик)
void pause(bool pause) {
    if (!pause)
        return;
    printf("Press ENTER to continue...");
    getchar();

}

int main(int argc, const char *argv[]) {
    bool use_pause = false;
    if (argc > 1 && !strcmp("--use-pause", argv[1]))
        use_pause = true;

    printf("Starting allocating...\n");
    pause(use_pause);
    __auto_type allocated = (void *) malloc(kMallocSize);
    if (!allocated) {
        fprintf(stderr, "Couldn't allocate memory. Aborting.\n");
        goto aborting;
    }
    printf("Successfully allocated %zu bytes at %lx\n", kMallocSize, (size_t) allocated);
    pause(use_pause);

    printf("Starting filling the memory...\n");
    __auto_type result = fill_memory(allocated, kMallocSize, kWriteThreadsCount);
    if (!result) {
        fprintf(stderr, "Couldn't fill the memory from /dev/urandom. Aborting.\n");
        goto aborting;
    }
    printf("Successfully filled with %zu threads\n", kWriteThreadsCount);
    pause(use_pause);

    printf("Writing files on disk...\n");
    __auto_type filenames = write_on_disk(allocated, kMallocSize, kBlockSize, kWriteSize);
    if (!filenames.filenames) {
        fprintf(stderr, "Couldn't write files on the disk, aborting.\n");
        goto aborting;
    }

    pause(use_pause);

    printf("Reading the files...\n");
    __auto_type aggreagated = aggregate_data(kReadThreadsCount, filenames);
    printf("Sum of bytes (signed char count): %zi\n", aggreagated);
    pause(use_pause);

    delete_files_if_exist(&filenames);
    free_filelist(&filenames);
    printf("Deallocating the memory...\n");
    free(allocated);
    printf("Successfully freed.\n");
    pause(use_pause);
    return 0;

    aborting:
    {
        delete_files_if_exist(&filenames);
        free_filelist(&filenames);
        free(allocated);
        return 1;
    }
}