//
// Created by thiagorigonatti on 02/08/25.
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#include "settings.h"
#include "time.h"

#define MAX_EVENTS 64
#define READ_BUF_SIZE 384
#define MAX_FD 128

static const char RESPONSE_POST[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";

typedef struct {
    int fd;
    const char *write_buf;
    size_t write_len;
    size_t write_offset;
} client_state_t;

static volatile sig_atomic_t running = 1;

static void sigint_handler(int _) {
    (void) _;
    running = 0;
}

static int make_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static client_state_t clients[MAX_FD];

static void close_client(int fd, int epoll_fd) {
    if (fd < 0 || fd >= MAX_FD) return;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    clients[fd].fd = -1;
    clients[fd].write_buf = NULL;
    clients[fd].write_len = 0;
    clients[fd].write_offset = 0;
}

static void start_write(int fd, int epoll_fd) {
    client_state_t *client = &clients[fd];
    while (client->write_offset < client->write_len) {
        ssize_t sent = send(fd,
                            client->write_buf + client->write_offset,
                            client->write_len - client->write_offset,
                            MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                close_client(fd, epoll_fd);
                return;
            }
        }
        client->write_offset += sent;
    }

    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLET | EPOLLRDHUP,
        .data.fd = fd
    };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        close_client(fd, epoll_fd);
        return;
    }
    client->write_buf = NULL;
    client->write_len = 0;
    client->write_offset = 0;
}

static void handle_read_event(int fd, int epoll_fd, const Settings *settings) {
    char buf[READ_BUF_SIZE];
    ssize_t count;
    size_t total_read = 0;

    while (1) {
        count = recv(fd, buf + total_read, sizeof(buf) - total_read - 1, 0);
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close_client(fd, epoll_fd);
            return;
        } else if (count == 0) {
            close_client(fd, epoll_fd);
            return;
        }
        total_read += count;
        if (total_read >= sizeof(buf) - 1) break;
    }
    if (total_read == 0) return;

    buf[total_read] = '\0';

    const char *response = NULL;
    size_t response_len = 0;

    if (memcmp(buf, "GET ", 4) == 0) {
        const char *path_start = buf + 4;
        const char *path_end = strchr(path_start, ' ');
        if (!path_end) return;

        size_t path_len = (size_t) (path_end - path_start);
        const char *expected_path = "/payments-summary?";
        size_t expected_len = strlen(expected_path);

        if (path_len >= expected_len && memcmp(path_start, expected_path, expected_len) == 0) {
            const char *query = path_start + expected_len;

            char from_iso[32] = {0};
            char to_iso[32] = {0};

            const char *p = query;
            while (*p && *p != ' ') {
                if (memcmp(p, "from=", 5) == 0) {
                    p += 5;
                    char *dst = from_iso;
                    while (*p && *p != '&' && *p != ' ' && (size_t) (dst - from_iso) < sizeof(from_iso) - 1) {
                        *dst++ = *p++;
                    }
                    *dst = '\0';
                } else if (memcmp(p, "to=", 3) == 0) {
                    p += 3;
                    char *dst = to_iso;
                    while (*p && *p != '&' && *p != ' ' && (size_t) (dst - to_iso) < sizeof(to_iso) - 1) {
                        *dst++ = *p++;
                    }
                    *dst = '\0';
                } else {
                    while (*p && *p != '&' && *p != ' ') p++;
                    if (*p == '&') p++;
                }
            }

            double sum = shm_array_length_sum(settings->shm_array, iso_to_unix(from_iso), iso_to_unix(to_iso));
            size_t size = shm_array_length(settings->shm_array, iso_to_unix(from_iso), iso_to_unix(to_iso));

            char json_buffer[128];
            int json_len = snprintf(
                json_buffer, sizeof(json_buffer),
                "{\"default\":{\"totalRequests\":%zu,\"totalAmount\":%.2f},"
                "\"fallback\":{\"totalRequests\":0,\"totalAmount\":0.0}}",
                size, sum
            );


            static char http_response[256];
            response_len = snprintf(
                http_response, sizeof(http_response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                json_len, json_buffer
            );
            response = http_response;
        } else {
            size_t amount = shm_array_size(settings->shm_array);
            double sum = shm_array_length_sum(settings->shm_array, 0, millis_now());

            char json_buffer[128];
            int json_len = snprintf(
                json_buffer, sizeof(json_buffer),
                "{\"default\":{\"totalRequests\":%zu,\"totalAmount\":%.2f},"
                "\"fallback\":{\"totalRequests\":0,\"totalAmount\":0.0}}",
                amount, sum
            );

            static char http_response[256];
            response_len = snprintf(
                http_response, sizeof(http_response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                json_len, json_buffer
            );
            response = http_response;
        }

        clients[fd].fd = fd;
        clients[fd].write_buf = response;
        clients[fd].write_len = response_len;
        clients[fd].write_offset = 0;

        struct epoll_event ev = {
            .events = EPOLLOUT | EPOLLET | EPOLLRDHUP,
            .data.fd = fd
        };

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
            close_client(fd, epoll_fd);
            return;
        }

        start_write(fd, epoll_fd);
    } else if (memcmp(buf, "POST /payments", 14) == 0) {
        response = RESPONSE_POST;
        response_len = sizeof(RESPONSE_POST) - 1;

        clients[fd].fd = fd;
        clients[fd].write_buf = response;
        clients[fd].write_len = response_len;
        clients[fd].write_offset = 0;

        struct epoll_event ev = {
            .events = EPOLLOUT | EPOLLET | EPOLLRDHUP,
            .data.fd = fd
        };

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
            close_client(fd, epoll_fd);
            return;
        }

        start_write(fd, epoll_fd);

        const char *body = strstr(buf, "\r\n\r\n");
        if (!body) exit(EXIT_FAILURE);
        body += 4;

        size_t len = strlen(body);
        while (len > 0 && (body[len - 1] == '\r' || body[len - 1] == '\n')) {
            len--;
        }

        if (len > 127) len = 127;
        char *cleaned_body = malloc(len + 1);
        if (!cleaned_body) exit(EXIT_FAILURE);
        memcpy(cleaned_body, body, len);
        cleaned_body[len] = '\0';
        lock_free_queue_push(settings->lock_free_queue, cleaned_body);
    } else {
        response = RESPONSE_POST;
        response_len = sizeof(RESPONSE_POST) - 1;

        clients[fd].fd = fd;
        clients[fd].write_buf = response;
        clients[fd].write_len = response_len;
        clients[fd].write_offset = 0;

        struct epoll_event ev = {
            .events = EPOLLOUT | EPOLLET | EPOLLRDHUP,
            .data.fd = fd
        };

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
            close_client(fd, epoll_fd);
            return;
        }

        start_write(fd, epoll_fd);
    }
}

// Servidor unix socket non blocking io assíncrono e com epoll
void *start_http_server(void *arg) {


    sleep(5);

    Settings *settings = (Settings *) arg;

    int cpu_index = settings->index;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_index, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    unlink(settings->socket_path);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) return NULL;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, settings->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        close(listen_fd);
        return NULL;
    }

    chmod(settings->socket_path, 0777);

    if (make_non_blocking(listen_fd) == -1) {
        close(listen_fd);
        unlink(settings->socket_path);
        return NULL;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        close(listen_fd);
        unlink(settings->socket_path);
        return NULL;
    }

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        close(listen_fd);
        unlink(settings->socket_path);
        return NULL;
    }

    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLET,
        .data.fd = listen_fd
    };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        close(epoll_fd);
        close(listen_fd);
        unlink(settings->socket_path);
        return NULL;
    }

    for (int i = 0; i < MAX_FD; ++i) clients[i].fd = -1;

    printf("HTTP SERVER PRODUCER iniciado com sucesso, afinidade em cpu: %d\n", cpu_index);
    fflush(stdout);

    while (running) {
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t evflags = events[i].events;

            if (evflags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                close_client(fd, epoll_fd);
                continue;
            }

            if (fd == listen_fd) {
                while (1) {
                    int client_fd = accept(listen_fd, NULL, NULL);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        continue;
                    }
                    make_non_blocking(client_fd);
                    struct epoll_event cev = {
                        .events = EPOLLIN | EPOLLET | EPOLLRDHUP,
                        .data.fd = client_fd
                    };
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) == -1) {
                        close(client_fd);
                        continue;
                    }
                    clients[client_fd].fd = client_fd;
                    clients[client_fd].write_buf = NULL;
                    clients[client_fd].write_len = 0;
                    clients[client_fd].write_offset = 0;
                }
            } else {
                if (evflags & EPOLLIN) {
                    handle_read_event(fd, epoll_fd, settings);
                }
                if (evflags & EPOLLOUT) {
                    start_write(fd, epoll_fd);
                }
            }
        }
    }

    close(epoll_fd);
    close(listen_fd);
    unlink(settings->socket_path);
    return NULL;
}
