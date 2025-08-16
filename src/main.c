#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include "shm_array.h"
#include "settings.h"
#include "http_client.h"
#include "http_server.h"


#define QUEUE_CAPACITY 8500
#define ARRAY_CAPACITY 17000

#define SOCKET_PATH getenv("SOCKET_PATH")
#define SHM_PATH getenv("SHM_PATH")

int get_http_client_thread_amount() {
    const char *http_client_thread_amount = getenv("HTTP_CLIENT_THREADS");
    if (http_client_thread_amount) {
        int val = atoi(http_client_thread_amount);
        if (val > 0) return val;
    }
    return 1;
}

int get_http_server_thread_amount() {
    const char *http_server_thread_amount = getenv("HTTP_SERVER_THREADS");
    if (http_server_thread_amount) {
        int val = atoi(http_server_thread_amount);
        if (val > 0) return val;
    }
    return 1;
}

int get_index() {
    const char *index = getenv("INDEX");
    if (index) {
        return atoi(index);
    }
    exit(EXIT_FAILURE);
}

void cleanup() {
    unlink(SOCKET_PATH);
    shm_unlink(SHM_PATH);
}

void handle_signal(int sig) {
    cleanup();
    _exit(1);
}


int main() {
    atexit(cleanup);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGQUIT, handle_signal);

    Settings *settings = malloc(sizeof(Settings));
    settings->lock_free_queue = lock_free_queue_create(QUEUE_CAPACITY);
    settings->shm_array = shm_array_init(ARRAY_CAPACITY, SHM_PATH);
    settings->socket_path = SOCKET_PATH;
    settings->index = get_index();

    shm_array_clear(settings->shm_array);

    pthread_t server_threads[get_http_server_thread_amount()];
    for (int i = 0; i < get_http_server_thread_amount(); i++) {
        usleep(1000 * 200);
        pthread_create(&server_threads[i], NULL, start_http_server, settings);
    }

    pthread_t http_client_threads[get_http_client_thread_amount()];
    for (int i = 0; i < get_http_client_thread_amount(); i++) {
        usleep(1000 * 200);
        pthread_create(&http_client_threads[i], NULL, http_client_consumer, settings);
    }

    for (int i = 0; i < get_http_server_thread_amount(); i++) {
        pthread_join(server_threads[i], NULL);
    }

    for (int i = 0; i < get_http_client_thread_amount(); i++) {
        pthread_join(http_client_threads[i], NULL);
    }

    lock_free_queue_destroy(settings->lock_free_queue);
    shm_array_destroy(settings->shm_array, ARRAY_CAPACITY);
    free(settings);

    return 0;
}
