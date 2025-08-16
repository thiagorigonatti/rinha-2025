//
// Created by thiagorigonatti on 05/08/25.
//
#pragma once

#include "lock_free_queue.h"
#include "shm_array.h"

typedef struct {
    LockFreeQueue *lock_free_queue;
    SHMArray *shm_array;
    char *socket_path;
    int index;
} Settings;
