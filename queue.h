//
// Created by thiagorigonatti on 03/08/25.
//

#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    size_t capacity;
    atomic_size_t head;
    atomic_size_t tail;
    char** buffer;
} LockFreeQueue;

LockFreeQueue* lfqueue_create(size_t capacity);
void lfqueue_destroy(LockFreeQueue* q);

int lfqueue_push(LockFreeQueue* q, char* item);   // 0 = sucesso, -1 = cheia
char* lfqueue_pop(LockFreeQueue* q);              // NULL se vazia

#endif
