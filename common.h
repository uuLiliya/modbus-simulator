#ifndef COMMON_H
#define COMMON_H

/*
 * 公共头文件：集中声明服务器和客户端共享的常量以及网络通信所需的标准库。
 */

/* 标准库与系统头文件，提供输入输出、内存管理、套接字以及网络协议支持。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

/* 允许同时保持的最大客户端连接数。 */
#define MAX_CLIENTS 128
/* 应用层缓冲区大小，用于收发数据。 */
#define BUFFER_SIZE 4096
/* listen 系统调用的等待队列长度。 */
#define LISTEN_BACKLOG 128

#endif
