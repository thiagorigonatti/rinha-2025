//
// Created by thiagorigonatti on 05/08/25.
//

#ifndef RANGE_ARRAY_SHM_H
#define RANGE_ARRAY_SHM_H

#include <stddef.h>
#include <stdatomic.h>

#define SHM_NAME "/range_array_shm"


typedef struct {
    size_t capacity;
    _Atomic size_t size;
    double data[];
} RangeArraySHM;


RangeArraySHM *range_array_shm_init(size_t capacity);


void range_array_shm_destroy(RangeArraySHM *arr, size_t capacity);


int range_array_shm_insert(RangeArraySHM *arr, double value);


size_t range_array_shm_count_range(RangeArraySHM *arr, double min, double max);

void range_array_shm_clear(RangeArraySHM *arr, size_t capacity);

#endif
