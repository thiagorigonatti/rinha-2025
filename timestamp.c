//
// Created by thiagorigonatti on 02/08/25.
//

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "timestamp.h"

#include <sys/time.h>

void generate_requested_at_field(char *buffer, size_t buffer_size, double unix_time) {
    struct timespec ts;
    struct tm tm_utc;

    ts.tv_sec = (time_t)unix_time;
    ts.tv_nsec = (long)((unix_time - ts.tv_sec) * 1e9);

    gmtime_r(&ts.tv_sec, &tm_utc);

    int millis = ts.tv_nsec / 1000000;

    snprintf(buffer, buffer_size,
             "\"requestedAt\":\"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ\"",
             tm_utc.tm_year + 1900,
             tm_utc.tm_mon + 1,
             tm_utc.tm_mday,
             tm_utc.tm_hour,
             tm_utc.tm_min,
             tm_utc.tm_sec,
             millis);
}

int add_requested_at(const char *json_input, char *output, size_t output_size, double unix_time) {
    if (strstr(json_input, "\"requestedAt\"")) {
        if (strlen(json_input) >= output_size) {
            fprintf(stderr, "Erro: buffer insuficiente para copiar JSON original.\n");
            return -1;
        }
        strcpy(output, json_input);
        return 0;
    }

    const char *end = strrchr(json_input, '}');
    if (!end) {
        return -1;
    }

    char requested_at[64];
    generate_requested_at_field(requested_at, sizeof(requested_at), unix_time);

    size_t prefix_len = end - json_input;

    int written = snprintf(output, output_size,
        "%.*s,%s}", (int)prefix_len, json_input, requested_at);

    if (written < 0 || (size_t)written >= output_size) {
        fprintf(stderr, "Erro: buffer insuficiente ao adicionar campo.\n");
        return -1;
    }

    return 0;
}


double current_time_unix() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

double iso_to_unix(const char *datetime) {
    struct tm tm = {0};
    int milliseconds = 0;

    sscanf(datetime, "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &milliseconds);

    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    time_t seconds = timegm(&tm);

    return (double)seconds + (milliseconds / 1000.0);
}