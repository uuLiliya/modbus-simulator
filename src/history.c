/*
 * 命令历史记录管理实现
 * 
 * 功能描述：
 * - 实现命令历史的循环缓冲区存储
 * - 支持上下箭头键导航历史命令
 * - 提供终端raw mode的设置和恢复
 * - 实现交互式命令行输入，支持基本编辑功能
 */

#include "common.h"
#include <sys/select.h>
#include <ctype.h>

/*
 * 初始化命令历史结构
 * 参数：
 *   history - 命令历史结构指针
 */
void init_history(CommandHistory *history) {
    if (!history) {
        return;
    }
    
    memset(history, 0, sizeof(CommandHistory));
    history->count = 0;
    history->head = -1;
    history->current = -1;
    history->navigating = false;
}

/*
 * 添加命令到历史记录
 * 参数：
 *   history - 命令历史结构指针
 *   command - 要添加的命令字符串
 */
void add_to_history(CommandHistory *history, const char *command) {
    if (!history || !command) {
        return;
    }
    
    /* 跳过空命令和只有空白字符的命令 */
    const char *p = command;
    while (*p && isspace(*p)) {
        p++;
    }
    if (*p == '\0') {
        return;
    }
    
    /* 避免添加与最近一条相同的命令 */
    if (history->count > 0) {
        int last_idx = history->head;
        if (strcmp(history->commands[last_idx], command) == 0) {
            return;
        }
    }
    
    /* 更新head位置（循环） */
    history->head = (history->head + 1) % MAX_HISTORY_SIZE;
    
    /* 复制命令到缓冲区 */
    strncpy(history->commands[history->head], command, MAX_COMMAND_LENGTH - 1);
    history->commands[history->head][MAX_COMMAND_LENGTH - 1] = '\0';
    
    /* 更新计数（最多到MAX_HISTORY_SIZE） */
    if (history->count < MAX_HISTORY_SIZE) {
        history->count++;
    }
    
    /* 重置导航状态 */
    history->current = -1;
    history->navigating = false;
}

/*
 * 获取上一条历史命令
 * 参数：
 *   history - 命令历史结构指针
 * 返回：
 *   上一条命令字符串，如果没有则返回NULL
 */
const char* get_previous_command(CommandHistory *history) {
    if (!history || history->count == 0) {
        return NULL;
    }
    
    if (!history->navigating) {
        /* 开始导航，从最新的命令开始 */
        history->current = history->head;
        history->navigating = true;
        return history->commands[history->current];
    }
    
    /* 计算上一条命令的索引 */
    int prev_idx;
    if (history->count < MAX_HISTORY_SIZE) {
        /* 未填满缓冲区 */
        if (history->current == 0) {
            /* 已经到最早的命令 */
            return NULL;
        }
        prev_idx = history->current - 1;
    } else {
        /* 已填满缓冲区，需要循环 */
        int oldest_idx = (history->head + 1) % MAX_HISTORY_SIZE;
        if (history->current == oldest_idx) {
            /* 已经到最早的命令 */
            return NULL;
        }
        prev_idx = (history->current - 1 + MAX_HISTORY_SIZE) % MAX_HISTORY_SIZE;
    }
    
    history->current = prev_idx;
    return history->commands[history->current];
}

/*
 * 获取下一条历史命令
 * 参数：
 *   history - 命令历史结构指针
 * 返回：
 *   下一条命令字符串，如果没有则返回NULL
 */
const char* get_next_command(CommandHistory *history) {
    if (!history || !history->navigating) {
        return NULL;
    }
    
    /* 如果已经在最新位置，返回空字符串（清空输入） */
    if (history->current == history->head) {
        history->navigating = false;
        return "";
    }
    
    /* 计算下一条命令的索引 */
    int next_idx = (history->current + 1) % MAX_HISTORY_SIZE;
    history->current = next_idx;
    
    /* 如果回到最新位置，停止导航 */
    if (history->current == history->head) {
        history->navigating = false;
        return "";
    }
    
    return history->commands[history->current];
}

/*
 * 重置历史导航状态
 * 参数：
 *   history - 命令历史结构指针
 */
void reset_history_navigation(CommandHistory *history) {
    if (!history) {
        return;
    }
    history->navigating = false;
    history->current = -1;
}

/*
 * 启用终端raw模式
 * 参数：
 *   original - 用于保存原始终端设置的结构指针
 */
void enable_raw_mode(struct termios *original) {
    if (!original) {
        return;
    }
    
    /* 获取当前终端设置 */
    tcgetattr(STDIN_FILENO, original);
    
    /* 复制设置并修改 */
    struct termios raw = *original;
    
    /* 禁用规范模式（行缓冲）和回显 */
    raw.c_lflag &= ~(ICANON | ECHO);
    
    /* 设置最小读取字符数和超时 */
    raw.c_cc[VMIN] = 0;   /* 非阻塞读取 */
    raw.c_cc[VTIME] = 0;  /* 无超时 */
    
    /* 应用新设置 */
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/*
 * 恢复终端原始设置
 * 参数：
 *   original - 原始终端设置的结构指针
 */
void disable_raw_mode(struct termios *original) {
    if (!original) {
        return;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, original);
}

/*
 * 清除当前行并重新显示提示符和内容
 * 参数：
 *   prompt - 提示符字符串
 *   buffer - 当前输入缓冲区
 *   cursor_pos - 当前光标位置
 */
static void refresh_line(const char *prompt, const char *buffer, int cursor_pos) {
    /* 移动到行首 */
    printf("\r");
    /* 清除到行尾 */
    printf("\033[K");
    /* 显示提示符和内容 */
    printf("%s%s", prompt, buffer);
    
    /* 如果光标不在末尾，移动光标到正确位置 */
    int buffer_len = strlen(buffer);
    if (cursor_pos < buffer_len) {
        int diff = buffer_len - cursor_pos;
        printf("\033[%dD", diff);  /* 向左移动diff个字符 */
    }
    
    fflush(stdout);
}

/*
 * 读取一行输入，支持历史导航
 * 参数：
 *   buffer - 输入缓冲区
 *   buffer_size - 缓冲区大小
 *   prompt - 提示符字符串
 *   history - 命令历史结构指针
 *   socket_fd - socket文件描述符（用于检查服务器消息，-1表示不检查）
 * 返回：
 *   成功返回读取的字符数，失败返回-1，socket关闭返回-2
 */
int read_line_with_history(char *buffer, int buffer_size, const char *prompt, 
                           CommandHistory *history, int socket_fd) {
    if (!buffer || buffer_size <= 0 || !prompt || !history) {
        return -1;
    }
    
    struct termios original_termios;
    enable_raw_mode(&original_termios);
    
    memset(buffer, 0, buffer_size);
    int pos = 0;           /* 当前光标位置 */
    int len = 0;           /* 当前输入长度 */
    char temp_buffer[MAX_COMMAND_LENGTH];  /* 临时保存用户正在输入的内容 */
    memset(temp_buffer, 0, sizeof(temp_buffer));
    bool has_temp = false;
    
    /* 显示初始提示符 */
    printf("%s", prompt);
    fflush(stdout);
    
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        int max_fd = STDIN_FILENO;
        if (socket_fd >= 0) {
            FD_SET(socket_fd, &read_fds);
            max_fd = socket_fd > STDIN_FILENO ? socket_fd : STDIN_FILENO;
        }
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000;  /* 50ms超时，用于响应 */
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }
            disable_raw_mode(&original_termios);
            return -1;
        }
        
        /* 检查socket是否有数据（仅客户端使用） */
        if (socket_fd >= 0 && FD_ISSET(socket_fd, &read_fds)) {
            disable_raw_mode(&original_termios);
            return -2;  /* 表示socket有数据需要处理 */
        }
        
        /* 检查stdin */
        if (!FD_ISSET(STDIN_FILENO, &read_fds)) {
            continue;
        }
        
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            continue;
        }
        
        /* 处理转义序列（箭头键等） */
        if (c == 27) {  /* ESC */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
            
            if (seq[0] == '[') {
                if (seq[1] == 'A') {
                    /* 上箭头 */
                    const char *prev_cmd = get_previous_command(history);
                    if (prev_cmd) {
                        /* 第一次导航时保存当前输入 */
                        if (!has_temp && len > 0) {
                            strncpy(temp_buffer, buffer, sizeof(temp_buffer) - 1);
                            has_temp = true;
                        }
                        
                        /* 更新缓冲区 */
                        strncpy(buffer, prev_cmd, buffer_size - 1);
                        buffer[buffer_size - 1] = '\0';
                        len = strlen(buffer);
                        pos = len;
                        refresh_line(prompt, buffer, pos);
                    }
                } else if (seq[1] == 'B') {
                    /* 下箭头 */
                    if (history->navigating) {
                        const char *next_cmd = get_next_command(history);
                        if (next_cmd) {
                            if (next_cmd[0] == '\0' && has_temp) {
                                /* 恢复用户之前输入的内容 */
                                strncpy(buffer, temp_buffer, buffer_size - 1);
                                has_temp = false;
                            } else {
                                strncpy(buffer, next_cmd, buffer_size - 1);
                            }
                            buffer[buffer_size - 1] = '\0';
                            len = strlen(buffer);
                            pos = len;
                            refresh_line(prompt, buffer, pos);
                        }
                    }
                } else if (seq[1] == 'C') {
                    /* 右箭头 */
                    if (pos < len) {
                        pos++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                } else if (seq[1] == 'D') {
                    /* 左箭头 */
                    if (pos > 0) {
                        pos--;
                        printf("\033[D");
                        fflush(stdout);
                    }
                }
            }
        } else if (c == 127 || c == 8) {  /* Backspace */
            if (pos > 0) {
                /* 删除光标前的字符 */
                memmove(&buffer[pos - 1], &buffer[pos], len - pos + 1);
                pos--;
                len--;
                refresh_line(prompt, buffer, pos);
                reset_history_navigation(history);
            }
        } else if (c == '\n' || c == '\r') {
            /* 回车 */
            printf("\n");
            disable_raw_mode(&original_termios);
            
            /* 去除末尾的换行符 */
            buffer[len] = '\0';
            reset_history_navigation(history);
            return len;
        } else if (c == 4) {  /* Ctrl+D (EOF) */
            if (len == 0) {
                printf("\n");
                disable_raw_mode(&original_termios);
                return -1;
            }
        } else if (c == 3) {  /* Ctrl+C */
            printf("\n");
            disable_raw_mode(&original_termios);
            return -1;
        } else if (isprint(c)) {
            /* 可打印字符 */
            if (len < buffer_size - 1) {
                /* 在光标位置插入字符 */
                if (pos < len) {
                    memmove(&buffer[pos + 1], &buffer[pos], len - pos + 1);
                }
                buffer[pos] = c;
                pos++;
                len++;
                buffer[len] = '\0';
                refresh_line(prompt, buffer, pos);
                reset_history_navigation(history);
            }
        }
    }
    
    disable_raw_mode(&original_termios);
    return len;
}

/*
 * 初始化服务器输入状态
 * 参数：
 *   state - 服务器输入状态结构指针
 */
void init_server_input(ServerInputState *state) {
    if (!state) {
        return;
    }
    
    memset(state, 0, sizeof(ServerInputState));
    state->pos = 0;
    state->len = 0;
    state->has_temp = false;
    state->escape_state = 0;
    state->raw_mode_enabled = false;
    state->prompt_shown = false;
    
    /* 启用raw mode */
    enable_raw_mode(&state->original_termios);
    state->raw_mode_enabled = true;
}

/*
 * 清理服务器输入状态
 * 参数：
 *   state - 服务器输入状态结构指针
 */
void cleanup_server_input(ServerInputState *state) {
    if (!state) {
        return;
    }
    
    if (state->raw_mode_enabled) {
        disable_raw_mode(&state->original_termios);
        state->raw_mode_enabled = false;
    }
}

/*
 * 处理服务器输入的单个字符（非阻塞）
 * 参数：
 *   state - 服务器输入状态结构指针
 *   prompt - 提示符字符串
 *   history - 命令历史结构指针
 *   output - 输出缓冲区（当完成一行输入时）
 *   output_size - 输出缓冲区大小
 * 返回：
 *   0 - 继续输入中
 *   1 - 完成一行输入，结果在output中
 *   -1 - 错误
 */
int process_server_input_char(ServerInputState *state, const char *prompt, 
                              CommandHistory *history, char *output, int output_size) {
    if (!state || !prompt || !history || !output) {
        return -1;
    }
    
    /* 显示提示符（如果还未显示） */
    if (!state->prompt_shown) {
        printf("%s", prompt);
        fflush(stdout);
        state->prompt_shown = true;
    }
    
    /* 非阻塞读取一个字符 */
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return 0;  /* 没有数据可读，继续 */
    }
    
    /* 状态机处理转义序列 */
    if (state->escape_state == 1) {
        /* 已收到ESC，等待[ */
        if (c == '[') {
            state->escape_state = 2;
            return 0;
        } else {
            /* 不是完整的箭头键序列，重置 */
            state->escape_state = 0;
            return 0;
        }
    } else if (state->escape_state == 2) {
        /* 已收到ESC[，等待方向键 */
        state->escape_state = 0;  /* 重置状态 */
        
        if (c == 'A') {
            /* 上箭头 */
            const char *prev_cmd = get_previous_command(history);
            if (prev_cmd) {
                /* 第一次导航时保存当前输入 */
                if (!state->has_temp && state->len > 0) {
                    strncpy(state->temp_buffer, state->buffer, sizeof(state->temp_buffer) - 1);
                    state->temp_buffer[sizeof(state->temp_buffer) - 1] = '\0';
                    state->has_temp = true;
                }
                
                /* 更新缓冲区 */
                strncpy(state->buffer, prev_cmd, sizeof(state->buffer) - 1);
                state->buffer[sizeof(state->buffer) - 1] = '\0';
                state->len = strlen(state->buffer);
                state->pos = state->len;
                refresh_line(prompt, state->buffer, state->pos);
            }
            return 0;
        } else if (c == 'B') {
            /* 下箭头 */
            if (history->navigating) {
                const char *next_cmd = get_next_command(history);
                if (next_cmd) {
                    if (next_cmd[0] == '\0' && state->has_temp) {
                        /* 恢复用户之前输入的内容 */
                        strncpy(state->buffer, state->temp_buffer, sizeof(state->buffer) - 1);
                        state->buffer[sizeof(state->buffer) - 1] = '\0';
                        state->has_temp = false;
                    } else {
                        strncpy(state->buffer, next_cmd, sizeof(state->buffer) - 1);
                        state->buffer[sizeof(state->buffer) - 1] = '\0';
                    }
                    state->len = strlen(state->buffer);
                    state->pos = state->len;
                    refresh_line(prompt, state->buffer, state->pos);
                }
            }
            return 0;
        } else if (c == 'C') {
            /* 右箭头 */
            if (state->pos < state->len) {
                state->pos++;
                printf("\033[C");
                fflush(stdout);
            }
            return 0;
        } else if (c == 'D') {
            /* 左箭头 */
            if (state->pos > 0) {
                state->pos--;
                printf("\033[D");
                fflush(stdout);
            }
            return 0;
        }
        return 0;
    }
    
    /* 正常字符处理 */
    if (c == 27) {  /* ESC */
        state->escape_state = 1;
        return 0;
    } else if (c == 127 || c == 8) {  /* Backspace */
        if (state->pos > 0) {
            /* 删除光标前的字符 */
            memmove(&state->buffer[state->pos - 1], &state->buffer[state->pos], 
                   state->len - state->pos + 1);
            state->pos--;
            state->len--;
            refresh_line(prompt, state->buffer, state->pos);
            reset_history_navigation(history);
        }
        return 0;
    } else if (c == '\n' || c == '\r') {
        /* 回车 - 完成一行输入 */
        printf("\n");
        
        /* 复制到输出缓冲区 */
        strncpy(output, state->buffer, output_size - 1);
        output[output_size - 1] = '\0';
        
        /* 重置状态 */
        memset(state->buffer, 0, sizeof(state->buffer));
        state->pos = 0;
        state->len = 0;
        state->has_temp = false;
        state->prompt_shown = false;
        reset_history_navigation(history);
        
        return 1;  /* 表示完成一行输入 */
    } else if (c == 4) {  /* Ctrl+D */
        if (state->len == 0) {
            /* 空行时退出 */
            printf("\n");
            return -1;
        }
        return 0;
    } else if (c == 3) {  /* Ctrl+C */
        /* 取消当前输入 */
        printf("^C\n");
        memset(state->buffer, 0, sizeof(state->buffer));
        state->pos = 0;
        state->len = 0;
        state->has_temp = false;
        state->prompt_shown = false;
        reset_history_navigation(history);
        return 0;
    } else if (isprint(c)) {
        /* 可打印字符 */
        if (state->len < (int)sizeof(state->buffer) - 1) {
            /* 在光标位置插入字符 */
            if (state->pos < state->len) {
                memmove(&state->buffer[state->pos + 1], &state->buffer[state->pos], 
                       state->len - state->pos + 1);
            }
            state->buffer[state->pos] = c;
            state->pos++;
            state->len++;
            state->buffer[state->len] = '\0';
            refresh_line(prompt, state->buffer, state->pos);
            reset_history_navigation(history);
        }
        return 0;
    }
    
    return 0;
}
