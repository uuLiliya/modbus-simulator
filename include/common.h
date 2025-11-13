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

/* 终端控制相关函数 */
void enable_raw_mode(struct termios *original);
void disable_raw_mode(struct termios *original);

/* 命令历史管理函数 */
void init_history(CommandHistory *history);
void add_to_history(CommandHistory *history, const char *command);
const char* get_previous_command(CommandHistory *history);
const char* get_next_command(CommandHistory *history);
void reset_history_navigation(CommandHistory *history);

/* 交互式输入函数 */
int read_line_with_history(char *buffer, int buffer_size, const char *prompt, 
                           CommandHistory *history, int socket_fd);

/* 服务器非阻塞输入状态 */
typedef struct {
    char buffer[MAX_COMMAND_LENGTH];   /* 输入缓冲区 */
    int pos;                           /* 光标位置 */
    int len;                           /* 当前输入长度 */
    char temp_buffer[MAX_COMMAND_LENGTH]; /* 临时保存用户正在输入的内容 */
    bool has_temp;                     /* 是否有临时保存的内容 */
    int escape_state;                  /* 转义序列状态：0=正常, 1=收到ESC, 2=收到ESC[ */
    struct termios original_termios;   /* 原始终端设置 */
    bool raw_mode_enabled;             /* 是否已启用raw mode */
    bool prompt_shown;                 /* 是否已显示提示符 */
} ServerInputState;

/* 服务器非阻塞输入函数 */
void init_server_input(ServerInputState *state);
void cleanup_server_input(ServerInputState *state);
int process_server_input_char(ServerInputState *state, const char *prompt, 
                              CommandHistory *history, char *output, int output_size);

#endif
