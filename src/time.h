//
// Created by thiagorigonatti on 02/08/25.
//
#pragma once

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

// Retorna o tempo atual em milissegundos desde Unix Epoch
static inline uint64_t millis_now() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
    return (uint64_t) ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

// Converte timestamp em milissegundos para string ISO UTC
// e preenche o buffer 'out' com formato "YYYY-MM-DDTHH:mm:ss.sssZ"
static inline void millis_to_iso(uint64_t millis, char *out) {
    time_t seconds = millis / 1000;
    int ms = millis % 1000;

    struct tm tm_utc;
    gmtime_r(&seconds, &tm_utc);

    sprintf(out, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
            tm_utc.tm_year + 1900,
            tm_utc.tm_mon + 1,
            tm_utc.tm_mday,
            tm_utc.tm_hour,
            tm_utc.tm_min,
            tm_utc.tm_sec,
            ms);
}

// Insere o campo "requestedAt" com timestamp ISO no JSON
// faz cópias eficientes para construir a nova string JSON completa
static inline void add_requested_at_field(const char *json_base, const char *timestamp_iso, char *out) {
    const size_t base_len = strlen(json_base) - 1;
    static const size_t timestamp_len = 24;
    static const char suffix[] = ",\"requestedAt\":\"";
    static const size_t suffix_len = sizeof(suffix) - 1;
    static const char end_quote[] = "\"}";
    static const size_t end_quote_len = sizeof(end_quote) - 1;

    __builtin_memcpy(out, json_base, base_len);
    __builtin_memcpy(out + base_len, suffix, suffix_len);
    __builtin_memcpy(out + base_len + suffix_len, timestamp_iso, timestamp_len);
    __builtin_memcpy(out + base_len + suffix_len + timestamp_len, end_quote, end_quote_len);
    out[base_len + suffix_len + timestamp_len + end_quote_len] = '\0';
}


// Converte string ISO UTC para timestamp Unix em milissegundos
// faz parsing manual rápido com validação mínima de formato
// retorna UINT64_MAX em caso de erro ou formato inválido
static inline uint64_t iso_to_unix(const char *datetime) {
    if (!datetime) return UINT64_MAX;

    if (strlen(datetime) < 24) return UINT64_MAX;

    int y = (datetime[0] - '0') * 1000 + (datetime[1] - '0') * 100 + (datetime[2] - '0') * 10 + (datetime[3] - '0');
    int mo = (datetime[5] - '0') * 10 + (datetime[6] - '0');
    int d = (datetime[8] - '0') * 10 + (datetime[9] - '0');
    int hh = (datetime[11] - '0') * 10 + (datetime[12] - '0');
    int mm = (datetime[14] - '0') * 10 + (datetime[15] - '0');
    int ss = (datetime[17] - '0') * 10 + (datetime[18] - '0');
    int ms = (datetime[20] - '0') * 100 + (datetime[21] - '0') * 10 + (datetime[22] - '0');

    if (datetime[4] != '-' || datetime[7] != '-' || datetime[10] != 'T' ||
        datetime[13] != ':' || datetime[16] != ':' || datetime[19] != '.' || datetime[23] != 'Z')
        return UINT64_MAX;

    struct tm tm = {0};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = hh;
    tm.tm_min = mm;
    tm.tm_sec = ss;

    time_t t = timegm(&tm);
    if (t == -1) return UINT64_MAX;

    return (uint64_t) t * 1000ULL + ms;
}
