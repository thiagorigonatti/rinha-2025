//
// Created by thiagorigonatti on 05/08/25.
//

#define _GNU_SOURCE
#include "range_array.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

RangeArraySHM *range_array_shm_init(size_t capacity) {
    int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("shm_open");
        return NULL;
    }

    size_t shm_size = sizeof(RangeArraySHM) + sizeof(double) * capacity;

    if (ftruncate(fd, shm_size) == -1) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    void *mapped = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    RangeArraySHM *arr = (RangeArraySHM *)mapped;


    if (arr->capacity != capacity) {
        memset(arr, 0, shm_size);
        arr->capacity = capacity;
        atomic_store(&arr->size, 0);
    }

    return arr;
}

void range_array_shm_destroy(RangeArraySHM *arr, size_t capacity) {
    if (!arr) return;

    size_t shm_size = sizeof(RangeArraySHM) + sizeof(double) * capacity;
    munmap(arr, shm_size);
    shm_unlink(SHM_NAME);
}

int range_array_shm_insert(RangeArraySHM *arr, double value) {
    size_t index = atomic_fetch_add(&arr->size, 1);
    if (index >= arr->capacity) {
        return -1;
    }
    arr->data[index] = value;
    return 0;
}

size_t range_array_shm_count_range(RangeArraySHM *arr, double min, double max) {
    size_t count = 0;
    size_t size = atomic_load(&arr->size);

    for (size_t i = 0; i < size; ++i) {
        double v = arr->data[i];
        if (v >= min && v <= max) {
            count++;
        }
    }
    return count;
}

void range_array_shm_clear(RangeArraySHM *arr, size_t capacity) {
    if (!arr) return;

    memset(arr->data, 0, sizeof(double) * capacity);
    atomic_store(&arr->size, 0);
}
