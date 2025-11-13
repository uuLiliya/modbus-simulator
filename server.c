/*
 * TCP 服务器实现
 * 
 * 功能描述：
 * - 基于 epoll 的高性能多客户端并发服务器
 * - 支持最多 128 个客户端同时连接
 * - 每个客户端使用 socket 文件描述符作为唯一标识
 * - 服务器可向指定客户端发送消息或广播消息
 * - 实现回显（Echo）协议，将客户端发来的消息前添加 "Echo: " 前缀后返回
 * - 使用非阻塞套接字和事件驱动模型提高吞吐量
 * - 支持优雅关闭和信号处理
 */

#include "common.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

/* epoll 每次可以处理的最大事件数 */
#define MAX_EVENTS 128

/* 全局变量：服务器套接字和 epoll 文件描述符 */
static int server_fd = -1;
static int epoll_fd = -1;

/* 全局变量：客户端信息数组和客户端计数器 */
static ClientInfo clients[MAX_CLIENTS];
static int client_count = 0;

/*
 * 初始化客户端信息数组
 */
static void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = false;
        memset(clients[i].id, 0, CLIENT_ID_LENGTH);
    }
}

/*
 * 根据文件描述符查找客户端信息
 * 参数：
 *   fd - 客户端文件描述符
 * 返回：
 *   指向客户端信息的指针，未找到返回NULL
 */
static ClientInfo* find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == fd) {
            return &clients[i];
        }
    }
    return NULL;
}

/*
 * 添加新客户端到客户端数组
 * 参数：
 *   fd - 客户端文件描述符
 *   addr - 客户端地址信息
 * 返回：
 *   指向新添加客户端信息的指针，失败返回NULL
 */
static ClientInfo* add_client(int fd, struct sockaddr_in addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].fd = fd;
            clients[i].addr = addr;
            clients[i].active = true;
            snprintf(clients[i].id, CLIENT_ID_LENGTH, "%d", fd);
            client_count++;
            return &clients[i];
        }
    }
    return NULL;
}

/*
 * 将客户端标记为非活跃状态
 * 参数：
 *   client - 客户端信息指针
 */
static void deactivate_client(ClientInfo *client) {
    if (!client || !client->active) {
        return;
    }
    client->active = false;
    client->fd = -1;
    memset(&client->addr, 0, sizeof(client->addr));
    memset(client->id, 0, CLIENT_ID_LENGTH);
    if (client_count > 0) {
        client_count--;
    }
}

/*
 * 断开与客户端的连接并清理 epoll/套接字资源
 * 参数：
 *   client - 客户端信息指针
 *   reason - 断开原因描述
 */
static void disconnect_client(ClientInfo *client, const char *reason) {
    if (!client || !client->active) {
        return;
    }

    int fd = client->fd;
    char addr_buf[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &client->addr.sin_addr, addr_buf, sizeof(addr_buf)) == NULL) {
        strncpy(addr_buf, "未知", sizeof(addr_buf) - 1);
    }
    int port = ntohs(client->addr.sin_port);

    if (epoll_fd != -1) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    }
    close(client->fd);

    deactivate_client(client);

    printf("[服务器] [fd:%d] 已断开连接（地址 %s:%d，原因: %s）（当前客户端总数: %d）\n",
           fd,
           addr_buf,
           port,
           reason ? reason : "未知",
           client_count);
}

/*
 * 列出所有连接的客户端
 */
static void list_clients() {
    printf("[服务器] 当前连接的客户端列表：\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            printf("  - [fd:%d] (地址=%s:%d)\n",
                   clients[i].fd,
                   inet_ntoa(clients[i].addr.sin_addr),
                   ntohs(clients[i].addr.sin_port));
        }
    }
    printf("[服务器] 总计：%d 个客户端\n", client_count);
}

/*
 * 向指定客户端发送消息
 * 参数：
 *   target_fd - 客户端的 socket 文件描述符
 *   message - 要发送的消息
 * 返回：
 *   成功返回true，失败返回false
 */
static bool send_to_client(int target_fd, const char *message) {
    ClientInfo *client = find_client_by_fd(target_fd);
    if (!client) {
        printf("[服务器] 错误：未找到文件描述符为 %d 的客户端\n", target_fd);
        return false;
    }

    if (!client->active) {
        printf("[服务器] 警告：文件描述符为 %d 的客户端已不在活跃状态\n", target_fd);
        return false;
    }

    ssize_t n_write = write(client->fd, message, strlen(message));
    if (n_write < 0) {
        perror("write");
        disconnect_client(client, "服务器发送失败");
        return false;
    }
    return true;
}

/*
 * 向所有客户端广播消息
 * 参数：
 *   message - 要广播的消息
 */
static void broadcast_message(const char *message) {
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            ssize_t n_write = write(clients[i].fd, message, strlen(message));
            if (n_write > 0) {
                sent_count++;
            }
        }
    }
    printf("[服务器] 已广播消息给 %d 个客户端\n", sent_count);
}

/*
 * 将文件描述符设置为非阻塞模式
 * 参数：
 *   fd - 需要设置的文件描述符
 */
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/*
 * 去除字符串末尾的换行符
 * 参数：
 *   str - 要处理的字符串
 */
static void trim_newline(char *str) {
    if (!str) {
        return;
    }
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

/*
 * 处理服务器命令行输入
 * 支持的命令：
 *   list - 列出所有客户端
 *   send <fd> <message> - 向指定文件描述符的客户端发送消息
 *   broadcast <message> - 向所有客户端广播消息
 *   help - 显示帮助信息
 */
static void handle_stdin_input() {
    char input[BUFFER_SIZE];
    if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
        return;
    }

    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    if (strlen(input) == 0) {
        return;
    }

    if (strcmp(input, "list") == 0) {
        list_clients();
    } else if (strcmp(input, "help") == 0) {
        printf("\n[服务器] 可用命令：\n");
        printf("  list                        - 列出所有连接的客户端\n");
        printf("  send <fd> <message>         - 向指定文件描述符的客户端发送消息\n");
        printf("  broadcast <message>         - 向所有客户端广播消息\n");
        printf("  help                        - 显示此帮助信息\n\n");
    } else if (strncmp(input, "send ", 5) == 0) {
        char *args = input + 5;
        char *space = strchr(args, ' ');
        if (space == NULL) {
            printf("[服务器] 错误：用法: send <fd> <message>\n");
            return;
        }
        *space = '\0';
        char *fd_str = args;
        char *message = space + 1;

        if (strlen(message) == 0) {
            printf("[服务器] 错误：消息不能为空\n");
            return;
        }

        char *endptr = NULL;
        errno = 0;
        long target_fd_long = strtol(fd_str, &endptr, 10);
        if (fd_str[0] == '\0' || endptr == NULL || *endptr != '\0' || errno != 0) {
            printf("[服务器] 错误：无效的文件描述符 %s\n", fd_str);
            return;
        }
        if (target_fd_long < 0 || target_fd_long > INT_MAX) {
            printf("[服务器] 错误：文件描述符超出范围\n");
            return;
        }
        int target_fd = (int)target_fd_long;

        char full_message[BUFFER_SIZE];
        size_t prefix_len = strlen("[服务器] ");
        size_t max_copy = BUFFER_SIZE - prefix_len - 2; /* 预留换行符和终止符 */
        size_t actual_len = strlen(message);
        if (actual_len > max_copy) {
            actual_len = max_copy;
        }
        snprintf(full_message, BUFFER_SIZE, "[服务器] %.*s\n", (int)actual_len, message);

        if (send_to_client(target_fd, full_message)) {
            printf("[服务器] 已向 [fd:%d] 发送消息: %.*s\n", target_fd, (int)actual_len, message);
        }
    } else if (strncmp(input, "broadcast ", 10) == 0) {
        char *message = input + 10;
        if (strlen(message) == 0) {
            printf("[服务器] 错误：消息不能为空\n");
            return;
        }

        char full_message[BUFFER_SIZE];
        size_t prefix_len = strlen("[服务器广播] ");
        size_t max_copy = BUFFER_SIZE - prefix_len - 2;
        size_t actual_len = strlen(message);
        if (actual_len > max_copy) {
            actual_len = max_copy;
        }
        snprintf(full_message, BUFFER_SIZE, "[服务器广播] %.*s\n", (int)actual_len, message);
        broadcast_message(full_message);
    } else {
        printf("[服务器] 未知命令: %s (输入 'help' 查看可用命令)\n", input);
    }
}

/*
 * 清理函数：在接收到退出信号时关闭所有资源
 * 参数：
 *   signum - 信号编号（未使用但为了兼容信号处理器函数签名）
 */
static void cleanup(int signum __attribute__((unused))) {
    printf("\n[服务器] 正在关闭...\n");
    if (epoll_fd != -1) {
        close(epoll_fd);
    }
    if (server_fd != -1) {
        close(server_fd);
    }
    exit(0);
}

/*
 * 主函数：初始化并运行 TCP 服务器
 * 
 * 执行流程：
 * 1. 解析命令行参数获取端口号
 * 2. 创建并配置服务器套接字
 * 3. 绑定端口并开始监听
 * 4. 创建 epoll 实例
 * 5. 进入事件循环处理连接和数据
 */
int main(int argc, char *argv[]) {
    /* 检查命令行参数 */
    if (argc != 2) {
        fprintf(stderr, "用法: %s <端口号>\n", argv[0]);
        exit(1);
    }

    /* 解析端口号并验证 */
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "错误: 无效的端口号。端口必须在 1 到 65535 之间。\n");
        exit(1);
    }

    /* 初始化客户端信息数组 */
    init_clients();

    /* 注册信号处理器，用于优雅关闭 */
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    /* 创建服务器套接字 */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    /* 设置套接字选项：允许地址快速重用 */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(1);
    }

    /* 配置服务器地址结构 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              /* IPv4 协议 */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* 监听所有网络接口 */
    server_addr.sin_port = htons(port);            /* 设置端口号（主机字节序转网络字节序） */

    /* 绑定服务器套接字到指定地址和端口 */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    /* 开始监听连接请求 */
    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    /* 将服务器套接字设置为非阻塞模式 */
    set_nonblocking(server_fd);

    /* 创建 epoll 实例 */
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        exit(1);
    }

    /* 将服务器套接字添加到 epoll 监听列表中 */
    struct epoll_event event;
    event.events = EPOLLIN;               /* 监听可读事件（即有新连接到来） */
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
        exit(1);
    }

    /* 将标准输入添加到 epoll 监听列表（用于服务器命令输入） */
    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;
    bool stdin_registered = true;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0) {
        fprintf(stderr, "[服务器] 警告：无法监听标准输入，命令功能不可用（原因: %s）。\n", strerror(errno));
        stdin_registered = false;
    }

    printf("[服务器] 正在监听端口 %d（最大客户端数: %d）\n", port, MAX_CLIENTS);
    if (stdin_registered) {
        printf("[服务器] 输入 'help' 查看可用命令\n\n");
    } else {
        printf("[服务器] 命令行控制不可用，将仅提供基础通信功能。\n\n");
    }

    /* 事件数组 */
    struct epoll_event events[MAX_EVENTS];

    /* 主事件循环 */
    while (1) {
        /* 等待事件发生（阻塞直到有事件或出错） */
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            /* 如果被信号中断，继续循环 */
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        /* 处理所有就绪的事件 */
        for (int i = 0; i < n; i++) {
            /* 情况一：标准输入有数据，处理服务器命令 */
            if (events[i].data.fd == STDIN_FILENO) {
                handle_stdin_input();
            }
            /* 情况二：服务器套接字有可读事件，表示有新连接到来 */
            else if (events[i].data.fd == server_fd) {
                /* 循环接受所有待处理的连接（非阻塞模式可能积累多个连接） */
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);

                    /* 接受客户端连接 */
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd < 0) {
                        /* 如果没有更多连接了，退出循环 */
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("accept");
                        break;
                    }

                    /* 检查是否达到最大客户端数量限制 */
                    if (client_count >= MAX_CLIENTS) {
                        printf("[服务器] 已达到最大客户端数量 (%d)。拒绝新连接。\n", MAX_CLIENTS);
                        close(client_fd);
                        continue;
                    }

                    /* 将客户端套接字设置为非阻塞模式 */
                    set_nonblocking(client_fd);

                    /* 将客户端套接字添加到 epoll 监听列表 */
                    struct epoll_event client_event;
                    client_event.events = EPOLLIN | EPOLLRDHUP; /* 监听可读和对端关闭事件 */
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        perror("epoll_ctl");
                        close(client_fd);
                        continue;
                    }

                    /* 添加客户端到管理数组 */
                    ClientInfo *client = add_client(client_fd, client_addr);
                    if (!client) {
                        printf("[服务器] 错误：无法添加客户端到管理列表\n");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        continue;
                    }

                    /* 打印连接信息 */
                    char addr_buf[INET_ADDRSTRLEN] = {0};
                    if (inet_ntop(AF_INET, &client_addr.sin_addr, addr_buf, sizeof(addr_buf)) == NULL) {
                        strncpy(addr_buf, "未知", sizeof(addr_buf) - 1);
                    }
                    printf("[服务器] [fd:%d] 客户端已连接，来自 %s:%d（当前客户端总数: %d）\n",
                           client->fd,
                           addr_buf,
                           ntohs(client_addr.sin_port),
                           client_count);

                    /* 发送欢迎消息 */
                    char welcome[BUFFER_SIZE];
                    snprintf(welcome, BUFFER_SIZE, "[服务器通知] 欢迎，您的文件描述符为 %d。\n", client->fd);
                    if (write(client_fd, welcome, strlen(welcome)) < 0) {
                        perror("write");
                        disconnect_client(client, "发送欢迎消息失败");
                    }
                }
            }
            /* 情况三：客户端套接字有数据或者发生断开 */
            else if (events[i].events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                int client_fd = events[i].data.fd;
                ClientInfo *client = find_client_by_fd(client_fd);
                if (!client) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }

                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    disconnect_client(client, "客户端异常断开");
                    continue;
                }

                char buffer[BUFFER_SIZE];
                ssize_t n_read = read(client_fd, buffer, BUFFER_SIZE - 1);

                if (n_read < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    perror("read");
                    disconnect_client(client, "读取失败");
                    continue;
                }

                if (n_read == 0 || (events[i].events & EPOLLRDHUP)) {
                    disconnect_client(client, "客户端关闭连接");
                    continue;
                }

                buffer[n_read] = '\0';

                char message[BUFFER_SIZE];
                strncpy(message, buffer, BUFFER_SIZE - 1);
                message[BUFFER_SIZE - 1] = '\0';
                trim_newline(message);

                const char *log_message = strlen(message) > 0 ? message : "(空消息)";
                printf("[服务器] [fd:%d] 消息：%s\n", client->fd, log_message);

                char response[BUFFER_SIZE];
                int resp_len = snprintf(response, BUFFER_SIZE, "[服务器回显][fd:%d] %s\n", client->fd, message);
                if (resp_len < 0) {
                    continue;
                }
                if (resp_len >= BUFFER_SIZE) {
                    response[BUFFER_SIZE - 1] = '\0';
                }

                ssize_t n_write = write(client_fd, response, strlen(response));
                if (n_write < 0) {
                    perror("write");
                    disconnect_client(client, "发送失败");
                }
            }
        }
    }

    /* 清理资源并退出 */
    cleanup(0);
    return 0;
}
