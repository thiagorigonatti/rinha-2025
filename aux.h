//
// Created by thiagorigonatti on 05/08/25.
//
#pragma once

#include "queue.h"
#include "range_array.h"

#ifndef AUX_H
#define AUX_H
typedef struct {
    LockFreeQueue * queue;
    RangeArraySHM * arr;
} Data;
#endif
