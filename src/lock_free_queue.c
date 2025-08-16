//
// Created by thiagorigonatti on 03/08/25.
//
#include <stdlib.h>
#include "lock_free_queue.h"
#include <unistd.h>


// cria e inicializa uma fila lock-free com capacidade fixa
// aloca memória para estrutura e buffer de ponteiros
// inicializa índices atômicos para cabeça e cauda da fila
// retorna ponteiro para fila criada ou NULL em falha.
LockFreeQueue *lock_free_queue_create(size_t capacity) {
    LockFreeQueue *queue = malloc(sizeof(LockFreeQueue));
    if (!queue) return NULL;

    queue->buffer = calloc(capacity, sizeof(char *));
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);

    return queue;
}

// Libera memória alocada para o buffer e a estrutura da fila
// ignora se o ponteiro da fila for NULL.
void lock_free_queue_destroy(LockFreeQueue *queue) {
    if (!queue) return;
    free(queue->buffer);
    free(queue);
}

// Tenta inserir um item na fila de forma lock-free
// retorna 0 se conseguiu, ou -1 se a fila estiver cheia
// usa operações atômicas para garantir segurança em concorrência.
int lock_free_queue_push(LockFreeQueue *queue, char *item) {

    size_t capacity = queue->capacity;

    while (1) {
        size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
        size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);

        if (tail - head >= capacity) {
            return -1;
        }

        if (atomic_compare_exchange_weak_explicit(&queue->tail, &tail, tail + 1,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed)
        ) {
            queue->buffer[tail % capacity] = item;
            return 0;
        }
    }
}

// Tenta remover um item da fila de forma lock-free
// retorna o item se disponível ou NULL se a fila estiver vazia
// usa operações atômicas para garantir exclusividade na remoção.
char *lock_free_queue_pop(LockFreeQueue *queue) {

    size_t capacity = queue->capacity;

    while (1) {
        size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
        size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

        if (head == tail) {
            return NULL;
        }

        if (atomic_compare_exchange_weak_explicit(&queue->head, &head, head + 1,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed)
        ) {
            return queue->buffer[head % capacity];
        }
    }
}
