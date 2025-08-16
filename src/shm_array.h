//
// Created by thiagorigonatti on 05/08/25.
//
#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>

#define SHM_MAGIC 0x53484D4152524159ULL

#include "payment.h"

typedef struct {
    uint64_t magic;
    size_t capacity;
    _Atomic uint64_t size_committed;
    pthread_mutex_t wlock;
    char _pad[32];
    Payment data[];
} SHMArray;


SHMArray *shm_array_init(size_t capacity, const char *shm_name);

void shm_array_destroy(SHMArray *array, uint64_t capacity);

int shm_array_insert(SHMArray *array, Payment value);

size_t shm_array_length(SHMArray *array, uint64_t min, uint64_t max);

double shm_array_length_sum(SHMArray *array, uint64_t min_ms, uint64_t max_ms);

void shm_array_clear(SHMArray *arr);

size_t shm_array_size(SHMArray *arr);
