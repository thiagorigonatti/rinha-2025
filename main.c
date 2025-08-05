//
// Created by thiagorigonatti on 02/08/25.
//

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "http_server.h"
#include "http_client.h"
#include "range_array.h"
#include "aux.h"



#define QUEUE_CAPACITY 10000
#define ARRAY_CAPACITY 20000

int get_http_client_threads() {
    const char* env_pthreads = getenv("PTHREADS");
    if (env_pthreads) {
        int val = atoi(env_pthreads);
        if (val > 0) return val;
    }
    return 4;
}

int main() {

    Data *data = malloc(sizeof(Data));
    data->queue = lfqueue_create(QUEUE_CAPACITY);
    data->arr = range_array_shm_init(ARRAY_CAPACITY);
    range_array_shm_clear(data->arr, ARRAY_CAPACITY);

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, start_server_single_thread, data) != 0) {
        fprintf(stderr, "Falha ao criar thread do servidor\n");
        return 1;
    }

    sleep(1);

    pthread_t http_client_threads[get_http_client_threads()];
    for (int i = 0; i < get_http_client_threads(); i++) {
        pthread_create(&http_client_threads[i], NULL, http_client_consumer, data);
    }

    for (int i = 0; i < get_http_client_threads(); i++) {
        pthread_join(http_client_threads[i], NULL);
    }

    pthread_join(server_thread, NULL);

    lfqueue_destroy(data->queue);
    range_array_shm_destroy(data->arr, ARRAY_CAPACITY);
    free(data);

    return 0;
}
