//
// Created by thiagorigonatti on 03/08/25.
//
#pragma once

#include <stdatomic.h>

typedef struct {
    size_t capacity;
    atomic_size_t head;
    atomic_size_t tail;
    char **buffer;
} LockFreeQueue;

LockFreeQueue *lock_free_queue_create(size_t capacity);

int lock_free_queue_push(LockFreeQueue *queue, char *item);

char *lock_free_queue_pop(LockFreeQueue *queue);

void lock_free_queue_destroy(LockFreeQueue *queue);
