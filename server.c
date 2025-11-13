#include "common.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_EVENTS 128

static int server_fd = -1;
static int epoll_fd = -1;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void cleanup(int signum __attribute__((unused))) {
    printf("\n[Server] Shutting down...\n");
    if (epoll_fd != -1) {
        close(epoll_fd);
    }
    if (server_fd != -1) {
        close(server_fd);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number. Port must be between 1 and 65535.\n");
        exit(1);
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    set_nonblocking(server_fd);

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        exit(1);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
        exit(1);
    }

    printf("[Server] Listening on port %d (Max clients: %d)\n", port, MAX_CLIENTS);

    struct epoll_event events[MAX_EVENTS];
    int client_count = 0;

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);

                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("accept");
                        break;
                    }

                    if (client_count >= MAX_CLIENTS) {
                        printf("[Server] Maximum clients reached (%d). Rejecting new connection.\n", MAX_CLIENTS);
                        close(client_fd);
                        continue;
                    }

                    set_nonblocking(client_fd);

                    struct epoll_event client_event;
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        perror("epoll_ctl");
                        close(client_fd);
                        continue;
                    }

                    client_count++;
                    printf("[Server] Client connected from %s:%d (Total clients: %d)\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_count);
                }
            } else if (events[i].events & (EPOLLIN | EPOLLRDHUP)) {
                int client_fd = events[i].data.fd;

                char buffer[BUFFER_SIZE];
                ssize_t n_read = read(client_fd, buffer, BUFFER_SIZE - 1);

                if (n_read < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        client_count--;
                        printf("[Server] Client disconnected (Total clients: %d)\n", client_count);
                    }
                } else if (n_read == 0 || (events[i].events & EPOLLRDHUP)) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    client_count--;
                    printf("[Server] Client disconnected (Total clients: %d)\n", client_count);
                } else {
                    buffer[n_read] = '\0';

                    printf("[Server] Received from client: %s", buffer);

                    char response[BUFFER_SIZE];
                    int resp_len = snprintf(response, BUFFER_SIZE - 1, "Echo: %s", buffer);
                    if (resp_len >= BUFFER_SIZE - 1) {
                        resp_len = BUFFER_SIZE - 1;
                    }
                    response[resp_len] = '\0';

                    ssize_t n_write = write(client_fd, response, strlen(response));
                    if (n_write < 0) {
                        perror("write");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        client_count--;
                        printf("[Server] Client disconnected (Total clients: %d)\n", client_count);
                    } else {
                        printf("[Server] Sent response to client\n");
                    }
                }
            }
        }
    }

    cleanup(0);
    return 0;
}
