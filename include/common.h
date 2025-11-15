#ifndef COMMON_H
#define COMMON_H

/*
 * 公共头文件：集中声明服务器和客户端共享的常量、数据结构以及网络通信所需的标准库。
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
#include <stdbool.h>
#include <termios.h>

/* 允许同时保持的最大客户端连接数。 */
#define MAX_CLIENTS 128
/* 应用层缓冲区大小，用于收发数据。 */
#define BUFFER_SIZE 4096
/* listen 系统调用的等待队列长度。 */
#define LISTEN_BACKLOG 128
/* 客户端标识符的最大长度。 */
#define CLIENT_ID_LENGTH 32

/* 命令历史记录相关常量 */
#define MAX_HISTORY_SIZE 100       /* 最大历史记录数量 */
#define MAX_COMMAND_LENGTH 1024    /* 单条命令的最大长度 */

/* 描述客户端会话的信息结构体。 */
typedef struct {
    int fd;                         /* 客户端对应的文件描述符。 */
    char id[CLIENT_ID_LENGTH];      /* 分配给客户端的唯一编号。 */
    struct sockaddr_in addr;        /* 客户端远端地址信息。 */
    bool active;                    /* 连接是否处于活跃状态。 */
} ClientInfo;

/* 命令历史记录管理结构体 */
typedef struct {
    char commands[MAX_HISTORY_SIZE][MAX_COMMAND_LENGTH];  /* 历史命令循环缓冲区 */
    int count;                      /* 当前已存储的历史命令总数 */
    int head;                       /* 循环缓冲区头部位置（最新命令） */
    int current;                    /* 当前浏览位置（用于上下导航） */
    bool navigating;                /* 是否正在浏览历史记录 */
} CommandHistory;

/* 输入状态机状态 */
typedef enum {
    INPUT_STATE_NORMAL = 0,      /* 正常字符输入 */
    INPUT_STATE_ESCAPE,          /* 收到ESC (0x1B) */
    INPUT_STATE_BRACKET,         /* 收到ESC[ */
    INPUT_STATE_BRACKET_PARAM    /* 收到ESC[N（N为参数） */
} InputState;

/* 行输入状态（服务器非阻塞输入使用） */
typedef struct {
    char buffer[MAX_COMMAND_LENGTH];       /* 输入缓冲区 */
    int pos;                               /* 光标位置 */
    int len;                               /* 当前输入长度 */
    char temp_buffer[MAX_COMMAND_LENGTH];  /* 保存未提交的输入行 */
    bool has_temp;                         /* 是否有临时保存的内容 */

    /* 有限状态机 */
    InputState state;                      /* 当前状态 */
    char escape_seq[8];                    /* 转义序列缓冲区 */
    int escape_pos;                        /* 转义序列位置 */

    /* 终端控制 */
    struct termios original_termios;       /* 原始终端设置 */
    bool raw_mode_enabled;                 /* 是否已启用raw mode */
    bool prompt_shown;                     /* 是否已显示提示符 */

    /* 窗口大小信息 */
    int term_width;                        /* 终端宽度 */
    int term_height;                       /* 终端高度 */
} InputLineState;

#endif
