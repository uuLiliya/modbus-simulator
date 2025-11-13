#include "common.h"
#include <signal.h>
#include <stdbool.h>

static int socket_fd = -1;

void cleanup(int signum __attribute__((unused))) {
    printf("\n[Client] Disconnecting...\n");
    if (socket_fd != -1) {
        close(socket_fd);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "Error: Invalid port number. Port must be between 1 and 65535.\n");
        exit(1);
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: Invalid server IP address.\n");
        close(socket_fd);
        exit(1);
    }

    printf("[Client] Connecting to %s:%d...\n", server_ip, server_port);

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(socket_fd);
        exit(1);
    }

    printf("[Client] Connected successfully!\n");
    printf("[Client] Type messages to send to server (type 'quit' to exit):\n\n");

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    while (1) {
        printf("[You] ");
        fflush(stdout);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        if (strcmp(buffer, "quit\n") == 0 || strcmp(buffer, "quit") == 0) {
            printf("[Client] Disconnecting...\n");
            break;
        }

        ssize_t n_write = write(socket_fd, buffer, strlen(buffer));
        if (n_write < 0) {
            perror("write");
            break;
        }

        memset(response, 0, BUFFER_SIZE);
        ssize_t n_read = read(socket_fd, response, BUFFER_SIZE - 1);
        if (n_read < 0) {
            perror("read");
            break;
        } else if (n_read == 0) {
            printf("[Client] Server closed connection.\n");
            break;
        } else {
            response[n_read] = '\0';
            printf("[Server] %s\n", response);
        }
    }

    close(socket_fd);
    socket_fd = -1;
    return 0;
}
