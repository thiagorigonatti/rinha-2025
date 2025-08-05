//
// Created by thiagorigonatti on 03/08/25.
//

#include "queue.h"
#include <stdlib.h>

LockFreeQueue* lfqueue_create(size_t capacity) {
    LockFreeQueue* q = malloc(sizeof(LockFreeQueue));
    if (!q) return NULL;

    q->buffer = calloc(capacity, sizeof(char*));
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->capacity = capacity;
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);

    return q;
}

void lfqueue_destroy(LockFreeQueue* q) {
    if (!q) return;
    free(q->buffer);
    free(q);
}

int lfqueue_push(LockFreeQueue* q, char* item) {
    size_t capacity = q->capacity;

    while (1) {
        size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
        size_t head = atomic_load_explicit(&q->head, memory_order_acquire);

        if ((tail - head) >= capacity) {
            return -1;  // cheia
        }

        if (atomic_compare_exchange_weak_explicit(
                &q->tail, &tail, tail + 1,
                memory_order_acq_rel, memory_order_relaxed)) {
            q->buffer[tail % capacity] = item;
            return 0;
                }
    }
}

char* lfqueue_pop(LockFreeQueue* q) {
    size_t capacity = q->capacity;

    while (1) {
        size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
        size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);

        if (head == tail) {
            return NULL;  // vazia
        }

        if (atomic_compare_exchange_weak_explicit(
                &q->head, &head, head + 1,
                memory_order_acq_rel, memory_order_relaxed)) {
            return q->buffer[head % capacity];
                }
    }
}
