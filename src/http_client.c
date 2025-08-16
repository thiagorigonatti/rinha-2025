//
// Created by thiagorigonatti on 02/08/25.
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/uio.h>

#include "lock_free_queue.h"
#include "shm_array.h"
#include "time.h"
#include "settings.h"


static int wait_for_event(int epfd, int sockfd, uint32_t events, int timeout_ms) {
    struct epoll_event ev = {0};
    ev.events = events;
    ev.data.fd = sockfd;


    if (epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev) == -1) {
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            return -1;
        }
    }

    struct epoll_event triggered_event;
    int n = epoll_wait(epfd, &triggered_event, 1, timeout_ms);
    if (n <= 0 || !(triggered_event.events & events)) {
        return -1;
    }
    return 0;
}

int http_post_json(int epfd, const struct addrinfo *res, const char *host, int port,
                   const char *path, const char *json_body, int timeout_ms) {


    int sockfd = socket(res->ai_family, SOCK_STREAM | SOCK_NONBLOCK, res->ai_protocol);
    if (sockfd < 0) return -1;

    int one = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    int64_t deadline = millis_now() + timeout_ms;

    int r = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (r < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return -1;
    }

    int64_t now = millis_now();
    if (now >= deadline) {
        close(sockfd);
        return -1;
    }


    if (wait_for_event(epfd, sockfd, EPOLLOUT, (int)(deadline - now)) < 0) {
        close(sockfd);
        return -1;
    }

    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0 || err != 0) {
        close(sockfd);
        return -1;
    }


    size_t body_len = strlen(json_body);
    char header_buf[256];
    int header_len = snprintf(header_buf, sizeof(header_buf),
                              "POST %s HTTP/1.1\r\n"
                              "Host: %s:%d\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              path, host, port, body_len);

    struct iovec iov[2];
    iov[0].iov_base = header_buf;
    iov[0].iov_len = header_len;
    iov[1].iov_base = (void*)json_body;
    iov[1].iov_len = body_len;

    size_t total_to_send = header_len + body_len;
    size_t sent = 0;

    while (sent < total_to_send) {
        now = millis_now();
        if (now >= deadline) {
            close(sockfd);
            return -1;
        }
        if (wait_for_event(epfd, sockfd, EPOLLOUT, (int)(deadline - now)) < 0) {
            close(sockfd);
            return -1;
        }

        ssize_t nsend = writev(sockfd, iov, 2);
        if (nsend < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            close(sockfd);
            return -1;
        }

        sent += nsend;

        if (sent < total_to_send) {
            if (sent < iov[0].iov_len) {
                iov[0].iov_base += nsend;
                iov[0].iov_len -= nsend;
            } else {
                size_t written_in_body = sent - iov[0].iov_len;
                iov[0].iov_len = 0;
                iov[1].iov_base = (void*)(json_body + written_in_body);
                iov[1].iov_len = body_len - written_in_body;
            }
        }
    }


    char buf[256];
    size_t pos = 0;
    while (pos < sizeof(buf) - 1) {
        now = millis_now();
        if (now >= deadline) {
            close(sockfd);
            return -1;
        }
        if (wait_for_event(epfd, sockfd, EPOLLIN, (int)(deadline - now)) < 0) {
            close(sockfd);
            return -1;
        }

        ssize_t nr = recv(sockfd, buf + pos, sizeof(buf) - 1 - pos, 0);
        if (nr <= 0) {
            close(sockfd);
            return -1;
        }
        pos += nr;
        buf[pos] = '\0';
        if (strstr(buf, "\r\n")) break;
    }

    close(sockfd);

    int status = -1;
    sscanf(buf, "HTTP/1.1 %d", &status);
    return status;
}


void *http_client_consumer(void *arg) {
    Settings *settings = (Settings *) arg;

    int cpu_index = settings->index + 2;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_index, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    const char *host = "payment-processor-default";
    const int port = 8080;
    const char *path = "/payments";
    const int timeout_ms = 5000;

    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        return NULL;
    }

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        perror("epoll_create1");
        freeaddrinfo(res);
        return NULL;
    }

    char json_buf[256];
    char iso_buf[32];

    printf("HTTP CLIENT CONSUMER iniciado com sucesso, afinidade em cpu: %d\n", cpu_index);
    fflush(stdout);

    http_post_json(epfd, res, host, port, path, "{\"correlationId\":\"357101b8-1d2a-4d9f-a51b-4dc1c7336033\",\"amount\":19.9,\"requestedAt\":\"2025-08-09T12:34:56.000Z\"}", timeout_ms);
    http_post_json(epfd, res, host, port, path, "{\"correlationId\":\"65b103b8-1d2a-fd9f-a41b-4dc1c7336031\",\"amount\":19.9,\"requestedAt\":\"2025-08-09T12:34:57.000Z\"}", timeout_ms);

    while (1) {
        char *msg = lock_free_queue_pop(settings->lock_free_queue);

        if (msg == NULL) {
            usleep(100);
            continue;
        }

        uint64_t now_ms = millis_now();
        millis_to_iso(now_ms, iso_buf);
        add_requested_at_field(msg, iso_buf, json_buf);

        int status = http_post_json(epfd, res, host, port, path, json_buf, timeout_ms);

        if (status == 200) {
            Payment payment = {0};
            json_to_payment_simple(msg, &payment);
            payment.requestedAt = now_ms;
            shm_array_insert(settings->shm_array, payment);
            free(msg);
        } else if (status == -1 || status == 500) {
            lock_free_queue_push(settings->lock_free_queue, msg);
        } else {
            free(msg);
        }
    }

    close(epfd);
    freeaddrinfo(res);
    return NULL;
}