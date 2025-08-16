//
// Created by thiagorigonatti on 05/08/25.
//
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/types.h>

#include "shm_array.h"



static int set_robust_pshared_mutex(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) return -1;

    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
#ifdef PTHREAD_MUTEX_ROBUST
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) {
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
#endif
    int rc = pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
    return rc == 0 ? 0 : -1;
}

SHMArray *shm_array_init(size_t capacity, const char *shm_name) {
    if (!shm_name || shm_name[0] != '/') {
        fprintf(stderr, "shm_array_init: shm_name deve começar com '/'. Ex: \"/payments\"\n");
        return NULL;
    }

    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        return NULL;
    }

    size_t total_size = sizeof(SHMArray) + capacity * sizeof(Payment);
    if (ftruncate(fd, (off_t)total_size) == -1) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }
    close(fd);


#ifdef MADV_WILLNEED
    madvise(addr, total_size, MADV_WILLNEED);
#endif
#ifdef MADV_HUGEPAGE
    madvise(addr, total_size, MADV_HUGEPAGE);
#endif

    SHMArray *array = (SHMArray*)addr;


    if (array->magic != SHM_MAGIC) {

        memset(addr, 0, total_size);

        if (set_robust_pshared_mutex(&array->wlock) != 0) {
            perror("pthread_mutex_init");
            munmap(addr, total_size);
            return NULL;
        }

        array->capacity = capacity;
        atomic_store_explicit(&array->size_committed, 0, memory_order_relaxed);

        __atomic_store_n(&array->magic, SHM_MAGIC, __ATOMIC_RELEASE);
    }

    return array;
}

void shm_array_destroy(SHMArray *array, uint64_t capacity) {
    if (!array) return;
    size_t total_size = sizeof(SHMArray) + capacity * sizeof(Payment);
    munmap(array, total_size);
}


int shm_array_insert(SHMArray *array, Payment value) {
    if (!array) return -1;

    int rc = pthread_mutex_lock(&array->wlock);
#ifdef PTHREAD_MUTEX_ROBUST
    if (rc == EOWNERDEAD) {
        pthread_mutex_consistent(&array->wlock);
        rc = 0;
    }
#endif
    if (rc != 0) return -1;

    uint64_t size = atomic_load_explicit(&array->size_committed, memory_order_relaxed);
    if (size >= array->capacity) {
        pthread_mutex_unlock(&array->wlock);
        return -1;
    }


    array->data[size] = value;


    atomic_store_explicit(&array->size_committed, size + 1, memory_order_release);

    pthread_mutex_unlock(&array->wlock);
    return 0;
}


size_t shm_array_length(SHMArray *array, uint64_t min, uint64_t max) {
    if (!array) return 0;
    uint64_t n = atomic_load_explicit(&array->size_committed, memory_order_acquire);

    size_t count = 0;
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t ts = array->data[i].requestedAt;
        if (ts >= min && ts <= max) {
            count++;
        }
    }
    return count;
}

double shm_array_length_sum(SHMArray *array, uint64_t min_ms, uint64_t max_ms) {
    if (!array) return 0.0;
    uint64_t n = atomic_load_explicit(&array->size_committed, memory_order_acquire);

    double sum = 0.0;
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t ts = array->data[i].requestedAt;
        if (ts >= min_ms && ts <= max_ms) {
            sum += array->data[i].value;
        }
    }
    return sum;
}


void shm_array_clear(SHMArray *arr) {
    if (!arr) return;
    int rc = pthread_mutex_lock(&arr->wlock);
#ifdef PTHREAD_MUTEX_ROBUST
    if (rc == EOWNERDEAD) {
        pthread_mutex_consistent(&arr->wlock);
        rc = 0;
    }
#endif
    if (rc != 0) return;

    atomic_store_explicit(&arr->size_committed, 0, memory_order_release);
    pthread_mutex_unlock(&arr->wlock);
}

size_t shm_array_size(SHMArray *arr) {
    if (!arr) return 0;
    return (size_t)atomic_load_explicit(&arr->size_committed, memory_order_acquire);
}
