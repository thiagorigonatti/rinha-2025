//
// Created by thiagorigonatti on 02/08/25.
//

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <signal.h>

#include "queue.h"
#include "aux.h"
#include "range_array.h"
#include "timestamp.h"

#define PORT 8080
#define MAX_EVENTS 8
#define READ_BUFFER_SIZE 2048

#define OK_200 "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
size_t counta = 0;

static int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

void *start_server_single_thread(void *arg) {
    Data *data = (Data *) arg;
    signal(SIGPIPE, SIG_IGN);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return NULL;

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(sfd, (struct sockaddr *) &addr, sizeof(addr)) < 0 ||
        listen(sfd, SOMAXCONN) < 0 ||
        make_socket_non_blocking(sfd) < 0) {
        close(sfd);
        return NULL;
    }

    int efd = epoll_create1(0);
    struct epoll_event event = {.events = EPOLLIN, .data.fd = sfd};
    epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);

    struct epoll_event *events = calloc(MAX_EVENTS, sizeof(struct epoll_event));

    printf("Servidor http iniciado na porta %d\n", PORT);

    while (1) {
        int n = epoll_wait(efd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }

            if (fd == sfd) {
                while (1) {
                    int client = accept(sfd, NULL, NULL);
                    if (client < 0) break;

                    make_socket_non_blocking(client);
                    event = (struct epoll_event){.events = EPOLLIN, .data.fd = client};
                    epoll_ctl(efd, EPOLL_CTL_ADD, client, &event);
                }
                continue;
            }

            char buf[READ_BUFFER_SIZE];
            ssize_t count = read(fd, buf, sizeof(buf) - 1);
            if (count <= 0) {
                epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }

            buf[count] = '\0';

            char method[8], path[256];

            sscanf(buf, "%7s %255s", method, path);

            char *body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4;

                if (strncmp(method, "POST", strlen("POST")) == 0) {
                    lfqueue_push(data->queue, strdup(body));
                    write(fd, OK_200, strlen(OK_200));
                } else if (strncmp(method, "GET", strlen("GET")) == 0) {
                    if (strncmp(path, "/payments-summary?", strlen("/payments-summary?")) == 0) {
                        char from[64] = {0};
                        char to[64] = {0};

                        sscanf(path, "/payments-summary?from=%[^&]&to=%s", from, to);

                        const size_t s = range_array_shm_count_range(data->arr, iso_to_unix(from), iso_to_unix(to));
                        const double d = s * 19.90;
                        char json_buffer[128];
                        sprintf(json_buffer,
                                "{\"default\":{\"totalRequests\":%lu,\"totalAmount\":%.2f},\"fallback\":{\"totalRequests\":0,\"totalAmount\":0.0}}",
                                s, d);

                        char json_buffer2[512]; // buffer maior para cabeçalho + JSON
                        int header_len = snprintf(json_buffer2, sizeof(json_buffer2),
                                                  "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nContent-Type: application/json\r\n\r\n",
                                                  strlen(json_buffer));

                        if (header_len > 0 && header_len < (int) sizeof(json_buffer2)) {
                            strncat(json_buffer2, json_buffer, sizeof(json_buffer2) - strlen(json_buffer2) - 1);
                        }
                        write(fd, json_buffer2, strlen(json_buffer2));
                    }
                }
            }
        }
    }

    free(events);
    close(efd);
    close(sfd);
    return 0;
}
