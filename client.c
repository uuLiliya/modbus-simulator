/*
 * TCP 客户端实现
 *
 * 功能描述：
 * - 连接指定的服务器地址和端口
 * - 从命令行读取用户输入并发送给服务器
 * - 接收并打印服务器返回的回显消息
 * - 支持 "quit" 命令和信号中断时的优雅退出
 */

#include "common.h"
#include <signal.h>
#include <stdbool.h>

/* 客户端套接字文件描述符（用于全局清理） */
static int socket_fd = -1;

/*
 * 清理函数：关闭 socket 并退出进程
 * 参数：
 *   signum - 信号编号（未使用，仅用于兼容信号处理器）
 */
void cleanup(int signum __attribute__((unused))) {
    printf("\n[客户端] 正在断开连接...\n");
    if (socket_fd != -1) {
        close(socket_fd);
    }
    exit(0);
}

/*
 * 主函数：读取参数、初始化网络连接并处理用户交互。
 */
int main(int argc, char *argv[]) {
    /* 检查命令行参数：需要服务器 IP 和端口号 */
    if (argc != 3) {
        fprintf(stderr, "用法: %s <服务器IP> <服务器端口>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    /* 验证端口号合法性 */
    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "错误: 无效的端口号。端口必须在 1 到 65535 之间。\n");
        exit(1);
    }

    /* 注册信号处理器，支持 Ctrl+C 等信号的优雅退出 */
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    /* 创建 TCP 套接字 */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(1);
    }

    /* 构造服务器地址结构 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    /* 将字符串形式的 IP 地址转换为网络字节序 */
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "错误: 无效的服务器 IP 地址。\n");
        close(socket_fd);
        exit(1);
    }

    printf("[客户端] 正在连接 %s:%d...\n", server_ip, server_port);

    /* 建立到服务器的 TCP 连接 */
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(socket_fd);
        exit(1);
    }

    printf("[客户端] 连接成功！\n");
    printf("[客户端] 输入消息发送给服务器（输入 'quit' 退出）：\n\n");

    char buffer[BUFFER_SIZE];   /* 用户输入缓冲区 */
    char response[BUFFER_SIZE]; /* 服务器响应缓冲区 */

    /* 主交互循环：读取用户输入，发送并接收回显 */
    while (1) {
        printf("[你] ");
        fflush(stdout);

        /* 从标准输入读取一行数据 */
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        /* 判断是否输入退出命令 */
        if (strcmp(buffer, "quit\n") == 0 || strcmp(buffer, "quit") == 0) {
            printf("[客户端] 正在断开连接...\n");
            break;
        }

        /* 将输入数据发送给服务器 */
        ssize_t n_write = write(socket_fd, buffer, strlen(buffer));
        if (n_write < 0) {
            perror("write");
            break;
        }

        /* 清空响应缓冲区并等待服务器回显消息 */
        memset(response, 0, BUFFER_SIZE);
        ssize_t n_read = read(socket_fd, response, BUFFER_SIZE - 1);
        if (n_read < 0) {
            perror("read");
            break;
        } else if (n_read == 0) {
            printf("[客户端] 服务器已关闭连接。\n");
            break;
        } else {
            response[n_read] = '\0';
            printf("[服务器] %s\n", response);
        }
    }

    /* 释放资源并退出 */
    close(socket_fd);
    socket_fd = -1;
    return 0;
}
