//
// Created by thiagorigonatti on 02/08/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include "queue.h"

#define _POSIX_C_SOURCE 200112L


#include <poll.h>
#include <stdatomic.h>

#include "range_array.h"
#include "timestamp.h"
#include "aux.h"

int http_request(const char *host, int port, const char *path, const char *method,
                 const char *body, int timeout_ms) {
    struct addrinfo hints = {0}, *res;
    int sockfd;


    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }


    sockfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }


    int r = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (r < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    struct pollfd pfd = {.fd = sockfd, .events = POLLOUT};
    r = poll(&pfd, 1, timeout_ms);
    if (r <= 0) {
        fprintf(stderr, r == 0 ? "Connection timeout\n" : "poll error: %s\n", strerror(errno));
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }


    const char *content_type = "application/json";
    size_t body_len = body ? strlen(body) : 0;

    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "%s %s HTTP/1.1\r\n"
                              "Host: %s:%d\r\n"
                              "Connection: close\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n",
                              method, path, host, port, content_type, body_len);


    if (write(sockfd, header, header_len) != header_len) {
        perror("write header");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }


    if (body_len > 0) {
        if (write(sockfd, body, body_len) != (ssize_t) body_len) {
            perror("write body");
            close(sockfd);
            freeaddrinfo(res);
            return -1;
        }
    }


    char response[2048] = {0};
    struct pollfd rfd = {.fd = sockfd, .events = POLLIN};

    int total_read = 0;
    int status_code = -1;

    if (poll(&rfd, 1, timeout_ms) > 0) {
        r = read(sockfd, response, sizeof(response) - 1);
        if (r > 0) {
            response[r] = '\0';


            char *status_line = strtok(response, "\r\n");
            if (status_line) {
                int major, minor;
                if (sscanf(status_line, "HTTP/%d.%d %d", &major, &minor, &status_code) == 3) {

                } else {
                    fprintf(stderr, "Malformed status line: %s\n", status_line);
                }
            }
        } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
        }
    } else {
        //fprintf(stderr, "No response received within timeout\n");
    }

    close(sockfd);
    freeaddrinfo(res);
    return status_code;
}


void *http_client_consumer(void *arg) {
    Data *data = (Data *) arg;

    printf("HTTP CLIENT iniciado com sucesso!\n");
    fflush(stdout);
    const char *host = "payment-processor-default";
    int port = 8080;
    const char *path = "/payments";
    const char *method = "POST";
    int timeout_ms = 5000;

    while (1) {
        char *msg = lfqueue_pop(data->queue); // tenta consumir uma mensagem
        usleep(2000);
        if (msg != NULL) {
            double current_time = current_time_unix();
            char new[128];
                add_requested_at(msg, new,  128, current_time);
            int status = http_request(host, port, path, method, new, timeout_ms);
            if (status == -1 || status == 500) {
                lfqueue_push(data->queue, msg);
            } else if (status == 200) {
                range_array_shm_insert(data->arr, current_time);
            }
        }
        //free(msg);
    }

    return NULL;
}
