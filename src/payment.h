//
// Created by thiagorigonatti on 14/08/25.
//
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    char id[37];
    double value;
    uint64_t requestedAt;
} Payment;

inline int extract_json_string(const char *json, const char *key, char *dest, size_t max_len) {
    const char *pos = strstr(json, key);
    if (!pos) return -1;

    pos = strchr(pos, ':');
    if (!pos) return -1;
    pos++;

    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos != '"') return -1;
    pos++;

    const char *end = strchr(pos, '"');
    if (!end) return -1;

    size_t len = end - pos;
    if (len >= max_len) len = max_len - 1;

    strncpy(dest, pos, len);
    dest[len] = '\0';
    return 0;
}

inline int extract_json_double(const char *json, const char *key, double *value) {
    const char *pos = strstr(json, key);
    if (!pos) return -1;

    pos = strchr(pos, ':');
    if (!pos) return -1;
    pos++;

    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;

    char *endptr;
    *value = strtod(pos, &endptr);
    if (pos == endptr) return -1;

    return 0;
}

static int json_to_payment_simple(const char *json_str, Payment *p) {
    if (!json_str || !p) return -1;

    if (extract_json_string(json_str, "correlationId", p->id, sizeof(p->id)) != 0)
        return -1;

    if (extract_json_double(json_str, "amount", &p->value) != 0)
        return -1;

    return 0;
}
