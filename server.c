/*
 * TCP 服务器实现
 * 
 * 功能描述：
 * - 基于 epoll 的高性能多客户端并发服务器
 * - 支持最多 128 个客户端同时连接
 * - 实现回显（Echo）协议，将客户端发来的消息前添加 "Echo: " 前缀后返回
 * - 使用非阻塞套接字和事件驱动模型提高吞吐量
 * - 支持优雅关闭和信号处理
 */

#include "common.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

/* epoll 每次可以处理的最大事件数 */
#define MAX_EVENTS 128

/* 全局变量：服务器套接字和 epoll 文件描述符 */
static int server_fd = -1;
static int epoll_fd = -1;

/*
 * 将文件描述符设置为非阻塞模式
 * 参数：
 *   fd - 需要设置的文件描述符
 */
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/*
 * 清理函数：在接收到退出信号时关闭所有资源
 * 参数：
 *   signum - 信号编号（未使用但为了兼容信号处理器函数签名）
 */
void cleanup(int signum __attribute__((unused))) {
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

    printf("[服务器] 正在监听端口 %d（最大客户端数: %d）\n", port, MAX_CLIENTS);

    /* 事件数组和客户端计数器 */
    struct epoll_event events[MAX_EVENTS];
    int client_count = 0;

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
            /* 情况一：服务器套接字有可读事件，表示有新连接到来 */
            if (events[i].data.fd == server_fd) {
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

                    /* 更新客户端计数并打印连接信息 */
                    client_count++;
                    printf("[服务器] 客户端已连接，来自 %s:%d（当前客户端总数: %d）\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_count);
                }
            }
            /* 情况二：客户端套接字有可读事件或对端关闭事件 */
            else if (events[i].events & (EPOLLIN | EPOLLRDHUP)) {
                int client_fd = events[i].data.fd;

                /* 从客户端读取数据 */
                char buffer[BUFFER_SIZE];
                ssize_t n_read = read(client_fd, buffer, BUFFER_SIZE - 1);

                /* 读取出错 */
                if (n_read < 0) {
                    /* 如果不是 EAGAIN 或 EWOULDBLOCK，说明真的出错了 */
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        client_count--;
                        printf("[服务器] 客户端已断开连接（当前客户端总数: %d）\n", client_count);
                    }
                }
                /* 读取到 0 字节或收到对端关闭事件，表示客户端断开连接 */
                else if (n_read == 0 || (events[i].events & EPOLLRDHUP)) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    client_count--;
                    printf("[服务器] 客户端已断开连接（当前客户端总数: %d）\n", client_count);
                }
                /* 成功读取到数据 */
                else {
                    /* 添加字符串结束符 */
                    buffer[n_read] = '\0';

                    printf("[服务器] 从客户端收到: %s", buffer);

                    /* 构造回显响应：在原消息前添加 "Echo: " 前缀 */
                    char response[BUFFER_SIZE];
                    int resp_len = snprintf(response, BUFFER_SIZE - 1, "Echo: %s", buffer);
                    /* 防止缓冲区溢出 */
                    if (resp_len >= BUFFER_SIZE - 1) {
                        resp_len = BUFFER_SIZE - 1;
                    }
                    response[resp_len] = '\0';

                    /* 发送响应给客户端 */
                    ssize_t n_write = write(client_fd, response, strlen(response));
                    if (n_write < 0) {
                        /* 写入失败，关闭连接 */
                        perror("write");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        client_count--;
                        printf("[服务器] 客户端已断开连接（当前客户端总数: %d）\n", client_count);
                    } else {
                        printf("[服务器] 已向客户端发送响应\n");
                    }
                }
            }
        }
    }

    /* 清理资源并退出 */
    cleanup(0);
    return 0;
}
