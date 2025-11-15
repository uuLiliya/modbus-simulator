/*
 * TCP 客户端实现
 *
 * 功能描述：
 * - 连接指定的服务器地址和端口
 * - 从命令行读取用户输入并发送给服务器
 * - 实时接收并显示服务器发送的消息（包括回显和服务器主动发送的消息）
 * - 支持 Modbus TCP 协议，可以发送 FC03 读寄存器和 FC06 写寄存器请求
 * - 使用select()同时监听标准输入和套接字
 * - 支持 "quit" 命令和信号中断时的优雅退出
 * 
 * 编译模式（通过 DEBUG_MODE 宏控制）：
 * - DEBUG_MODE=1（默认）：调试模式，显示所有调试消息
 * - DEBUG_MODE=0：纯数据流模式，仅接收 Modbus 数据，无调试输出
 */

#include "common.h"
#include "history.h"
#include "modbus.h"
#include <signal.h>
#include <sys/select.h>
#include <stdbool.h>

/* 如果未定义 DEBUG_MODE，默认为 1（调试模式） */
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

/* 客户端套接字文件描述符（用于全局清理） */
static int socket_fd = -1;

/* 全局事务ID计数器（用于 Modbus 请求） */
static uint16_t transaction_id = 1;

/* 命令历史记录 */
static CommandHistory cmd_history;

/*
 * 清理函数：关闭 socket 并退出进程
 * 参数：
 *   signum - 信号编号（未使用，仅用于兼容信号处理器）
 */
static void cleanup(int signum __attribute__((unused))) {
    printf("\n[客户端] 正在断开连接...\n");
    cleanup_history(&cmd_history);
    if (socket_fd != -1) {
        close(socket_fd);
    }
    exit(0);
}

/*
 * 处理 Modbus 响应
 * 
 * 参数：
 *   buffer - 接收到的数据缓冲区
 *   length - 数据长度
 */
static void handle_modbus_response(const uint8_t *buffer, size_t length) {
    ModbusTCPMessage response;
    
    /* 解析响应 */
    if (!modbus_parse_request(buffer, length, &response)) {
        printf("[客户端] Modbus 响应解析失败\n");
        return;
    }
    
    printf("[客户端] Modbus 响应：事务ID=%u, 功能码=0x%02X, 单元ID=%u\n",
           response.mbap.transaction_id, response.pdu.function_code, response.mbap.unit_id);
    
    /* 检查是否为错误响应 */
    if (response.pdu.function_code & MODBUS_FC_ERROR) {
        if (response.pdu.data_length >= 1) {
            uint8_t exception_code = response.pdu.data[0];
            printf("[客户端] Modbus 错误：%s (异常码: 0x%02X)\n",
                   modbus_get_exception_string(exception_code), exception_code);
        } else {
            printf("[客户端] Modbus 错误响应格式不正确\n");
        }
        return;
    }
    
    /* 根据功能码处理响应 */
    switch (response.pdu.function_code) {
        case MODBUS_FC_READ_HOLDING_REGISTERS: {
            uint16_t registers[MODBUS_MAX_READ_REGISTERS];
            uint16_t count = modbus_parse_fc03_response(&response, registers, MODBUS_MAX_READ_REGISTERS);
            
            if (count > 0) {
                printf("[客户端] FC03 读取成功，共 %u 个寄存器：\n", count);
                for (uint16_t i = 0; i < count; i++) {
                    printf("  寄存器[%u] = %u (0x%04X)\n", i, registers[i], registers[i]);
                }
            } else {
                printf("[客户端] FC03 响应解析失败\n");
            }
            break;
        }
        
        case MODBUS_FC_WRITE_SINGLE_REGISTER: {
            if (response.pdu.data_length >= 4) {
                uint16_t address = (uint16_t)(response.pdu.data[0] << 8) | response.pdu.data[1];
                uint16_t value = (uint16_t)(response.pdu.data[2] << 8) | response.pdu.data[3];
                printf("[客户端] FC06 写入成功：寄存器[%u] = %u (0x%04X)\n", address, value, value);
            } else {
                printf("[客户端] FC06 响应格式不正确\n");
            }
            break;
        }
        
        default:
            printf("[客户端] 未知的功能码响应：0x%02X\n", response.pdu.function_code);
            break;
    }
}

/*
 * 发送 Modbus FC03 读保持寄存器请求
 * 
 * 参数：
 *   start_address - 起始地址
 *   quantity - 寄存器数量
 * 
 * 返回：
 *   成功返回 true，失败返回 false
 */
static bool send_modbus_read_request(uint16_t start_address, uint16_t quantity) {
    uint8_t request_buffer[MODBUS_MAX_MESSAGE_LENGTH];
    
    /* 构建 FC03 请求 */
    size_t request_length = modbus_build_fc03_request(
        transaction_id++,
        0x01,  /* 单元ID */
        start_address,
        quantity,
        request_buffer,
        sizeof(request_buffer)
    );
    
    if (request_length == 0) {
        printf("[客户端] 构建 Modbus 读请求失败\n");
        return false;
    }
    
    /* 发送请求 */
    ssize_t n_write = write(socket_fd, request_buffer, request_length);
    if (n_write < 0) {
        perror("write");
        return false;
    }
    
    printf("[客户端] 已发送 FC03 读请求：起始地址=%u, 数量=%u (%zu 字节)\n",
           start_address, quantity, request_length);
    return true;
}

/*
 * 发送 Modbus FC06 写单个寄存器请求
 * 
 * 参数：
 *   register_address - 寄存器地址
 *   register_value - 寄存器值
 * 
 * 返回：
 *   成功返回 true，失败返回 false
 */
static bool send_modbus_write_request(uint16_t register_address, uint16_t register_value) {
    uint8_t request_buffer[MODBUS_MAX_MESSAGE_LENGTH];
    
    /* 构建 FC06 请求 */
    size_t request_length = modbus_build_fc06_request(
        transaction_id++,
        0x01,  /* 单元ID */
        register_address,
        register_value,
        request_buffer,
        sizeof(request_buffer)
    );
    
    if (request_length == 0) {
        printf("[客户端] 构建 Modbus 写请求失败\n");
        return false;
    }
    
    /* 发送请求 */
    ssize_t n_write = write(socket_fd, request_buffer, request_length);
    if (n_write < 0) {
        perror("write");
        return false;
    }
    
    printf("[客户端] 已发送 FC06 写请求：地址=%u, 值=%u (%zu 字节)\n",
           register_address, register_value, request_length);
    return true;
}

/*
 * 处理用户命令
 * 
 * 参数：
 *   input - 用户输入的命令字符串
 * 
 * 返回：
 *   如果是 quit 命令返回 false，否则返回 true
 */
static bool handle_user_command(const char *input) {
    /* 检查是否为退出命令 */
    if (strcmp(input, "quit\n") == 0 || strcmp(input, "quit") == 0) {
        return false;
    }
    
    /* 检查是否为 Modbus 命令 */
    if (strncmp(input, "modbus ", 7) == 0) {
        char cmd_type[32];
        int args[3];
        int num_args = sscanf(input, "modbus %31s %d %d", cmd_type, &args[0], &args[1]);
        
        if (num_args >= 2 && strcmp(cmd_type, "read") == 0) {
            /* modbus read <起始地址> <数量> */
            uint16_t start_address = (uint16_t)args[0];
            uint16_t quantity = (num_args >= 3) ? (uint16_t)args[1] : 1;
            
            if (quantity == 0 || quantity > MODBUS_MAX_READ_REGISTERS) {
                printf("[客户端] 错误：寄存器数量必须在 1 到 %d 之间\n", MODBUS_MAX_READ_REGISTERS);
                return true;
            }
            
            send_modbus_read_request(start_address, quantity);
        } else if (num_args >= 3 && strcmp(cmd_type, "write") == 0) {
            /* modbus write <地址> <值> */
            uint16_t register_address = (uint16_t)args[0];
            uint16_t register_value = (uint16_t)args[1];
            
            send_modbus_write_request(register_address, register_value);
        } else {
            printf("[客户端] 用法：\n");
            printf("  modbus read <起始地址> <数量>   - 读取保持寄存器 (FC03)\n");
            printf("  modbus write <地址> <值>        - 写单个寄存器 (FC06)\n");
            printf("示例：\n");
            printf("  modbus read 100 5    - 读取地址100开始的5个寄存器\n");
            printf("  modbus write 100 1234 - 将地址100的寄存器设为1234\n");
        }
        return true;
    }
    
    /* 普通文本消息，发送给服务器 */
    ssize_t n_write = write(socket_fd, input, strlen(input));
    if (n_write < 0) {
        perror("write");
        return false;
    }
    
    return true;
}

/*
 * 检查数据是否为 Modbus TCP 消息
 */
static bool is_modbus_response(const uint8_t *buffer, size_t length) {
    if (length < MODBUS_MBAP_HEADER_LENGTH) {
        return false;
    }
    
    /* 检查协议标识符（字节2-3）是否为0x0000 */
    uint16_t protocol_id = (uint16_t)(buffer[2] << 8) | buffer[3];
    return (protocol_id == MODBUS_PROTOCOL_ID);
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

#if DEBUG_MODE
    printf("[客户端] 连接成功！\n");
    printf("[客户端] 可用命令：\n");
    printf("  modbus read <起始地址> <数量>   - 读取保持寄存器 (FC03)\n");
    printf("  modbus write <地址> <值>        - 写单个寄存器 (FC06)\n");
    printf("  quit                             - 退出程序\n");
    printf("  或输入任意文本消息发送给服务器\n");
    printf("  使用上下箭头键导航命令历史\n\n");
#else
    printf("[客户端] 已连接到服务器 %s:%d\n\n", server_ip, server_port);
#endif

    /* 初始化命令历史 */
    init_history(&cmd_history);

    char buffer[BUFFER_SIZE];
    uint8_t response[BUFFER_SIZE];

    /* 主交互循环：使用历史导航功能读取用户输入 */
    while (1) {
        /* 读取一行输入，支持历史导航和实时接收服务器消息 */
        memset(buffer, 0, BUFFER_SIZE);
        int result = read_line_with_history(buffer, BUFFER_SIZE, "[你] ", &cmd_history, socket_fd);
        
        if (result == -2) {
            /* socket有数据，需要先处理服务器消息 */
            memset(response, 0, BUFFER_SIZE);
            ssize_t n_read = read(socket_fd, response, BUFFER_SIZE - 1);
            
            if (n_read < 0) {
                perror("read");
                break;
            } else if (n_read == 0) {
                printf("\n[客户端] 服务器已关闭连接。\n");
                break;
            } else {
                /* 检查是否为 Modbus 响应 */
                if (is_modbus_response(response, n_read)) {
                    printf("\n");
                    handle_modbus_response(response, n_read);
                } else {
                    /* 普通文本消息 */
                    response[n_read] = '\0';
                    printf("\n[服务器消息] %s", (char*)response);
                    if (response[n_read - 1] != '\n') {
                        printf("\n");
                    }
                }
                fflush(stdout);
            }
            continue;  /* 继续等待用户输入 */
        } else if (result < 0) {
            /* 读取失败或用户按Ctrl+C/Ctrl+D */
            printf("\n[客户端] 正在断开连接...\n");
            break;
        } else if (result == 0) {
            /* 空行，继续 */
            continue;
        }
        
        /* 添加到历史记录（非空命令） */
        if (buffer[0] != '\0') {
            add_to_history(&cmd_history, buffer);
        }
        
        /* 处理用户命令 */
        if (!handle_user_command(buffer)) {
            printf("[客户端] 正在断开连接...\n");
            break;
        }
    }

    /* 释放资源并退出 */
    cleanup_history(&cmd_history);
    close(socket_fd);
    socket_fd = -1;
    return 0;
}
