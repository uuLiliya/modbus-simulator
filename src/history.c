/*
 * 命令历史记录管理实现 - 按Linux readline标准实现
 * 
 * 功能描述：
 * - 有限状态机(FSM)处理Escape序列
 * - 循环队列存储最近100条命令
 * - 非阻塞I/O使用select()
 * - 完整的终端控制(raw mode, ANSI escape码)
 * - 行编辑功能(backspace, delete, 箭头键移动)
 * - 信号处理(SIGWINCH, SIGINT, SIGTSTP)
 */

#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "history.h"
#include <sys/select.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <signal.h>

/* 全局变量用于信号处理 */
volatile sig_atomic_t window_resized = 0;
volatile sig_atomic_t interrupted = 0;
volatile sig_atomic_t raw_mode_reenable = 0;
volatile sig_atomic_t handlers_need_reset = 0;

static struct termios saved_original_termios;
static volatile sig_atomic_t saved_termios_valid = 0;
static volatile sig_atomic_t raw_mode_active = 0;

/*
 * SIGWINCH信号处理器 - 终端窗口大小改变
 */
void handle_sigwinch(int sig __attribute__((unused))) {
    window_resized = 1;
}

/*
 * SIGINT信号处理器 - Ctrl+C
 */
void handle_sigint(int sig __attribute__((unused))) {
    interrupted = 1;
}

/*
 * SIGTSTP信号处理器 - Ctrl+Z
 */
void handle_sigtstp(int sig __attribute__((unused))) {
    /* 保存终端状态并切换回规范模式 */
    if (raw_mode_active && saved_termios_valid) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_original_termios);
        fflush(stdout);
        raw_mode_active = 0;
        raw_mode_reenable = 1;
    }
    
    /* 使用默认行为 - 暂停进程 */
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
    
    /* 进程恢复后重新安装信号处理器 */
    handlers_need_reset = 1;
}

/*
 * SIGCONT信号处理器 - 进程恢复
 */
void handle_sigcont(int sig __attribute__((unused))) {
    raw_mode_reenable = 1;
    handlers_need_reset = 1;
}

/*
 * 设置信号处理器
 */
void setup_signal_handlers(void) {
    struct sigaction sa;
    
    /* SIGWINCH - 窗口大小改变 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
    
    /* SIGINT - Ctrl+C */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    /* SIGTSTP - Ctrl+Z */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigtstp;
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);
    
    /* SIGCONT - 继续运行 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigcont;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCONT, &sa, NULL);
}

/*
 * 获取终端窗口大小
 */
static void get_terminal_size(int *width, int *height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    } else {
        *width = 80;   /* 默认宽度 */
        *height = 24;  /* 默认高度 */
    }
}

/*
 * 初始化命令历史结构
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
 * 清理命令历史结构
 */
void cleanup_history(CommandHistory *history) {
    if (!history) {
        return;
    }
    
    memset(history, 0, sizeof(CommandHistory));
}

/*
 * 添加命令到历史记录（去重、过滤空命令）
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
    
    /* 避免添加与最近一条相同的命令（去重） */
    if (history->count > 0) {
        int last_idx = history->head;
        if (strcmp(history->commands[last_idx], command) == 0) {
            return;
        }
    }
    
    /* 更新head位置（循环缓冲区） */
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
 * 禁用ICANON（行缓冲）、ECHO（回显）、ISIG（信号）
 */
void enable_raw_mode(struct termios *original) {
    if (!original) {
        return;
    }
    
    /* 获取当前终端设置 */
    tcgetattr(STDIN_FILENO, original);
    
    /* 保存副本用于信号处理 */
    if (!saved_termios_valid) {
        saved_original_termios = *original;
        saved_termios_valid = 1;
    }
    
    /* 复制设置并修改 */
    struct termios raw = *original;
    
    /* 禁用规范模式、回显和信号处理 */
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    
    /* 禁用特殊字符处理 */
    raw.c_iflag &= ~(IXON | ICRNL);
    
    /* 禁用输出处理 */
    raw.c_oflag &= ~(OPOST);
    
    /* 设置最小读取字符数和超时 */
    raw.c_cc[VMIN] = 0;   /* 非阻塞读取 */
    raw.c_cc[VTIME] = 0;  /* 无超时 */
    
    /* 应用新设置 */
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    raw_mode_active = 1;
}

/*
 * 恢复终端原始设置
 */
void disable_raw_mode(struct termios *original) {
    if (!original) {
        return;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, original);
    raw_mode_active = 0;
}

/*
 * 清除当前行
 */
void clear_line(void) {
    printf("\r\033[K");
    fflush(stdout);
}

/*
 * 移动光标到指定位置
 */
void move_cursor_to_pos(int current_pos, int target_pos) {
    if (target_pos < current_pos) {
        /* 向左移动 */
        int diff = current_pos - target_pos;
        printf("\033[%dD", diff);
    } else if (target_pos > current_pos) {
        /* 向右移动 */
        int diff = target_pos - current_pos;
        printf("\033[%dC", diff);
    }
    fflush(stdout);
}

/*
 * 刷新当前行显示
 * 使用ANSI escape码控制光标和清除
 */
void refresh_line(const char *prompt, const char *buffer, int cursor_pos) {
    /* 移动到行首并清除到行尾 */
    printf("\r\033[K");
    
    /* 显示提示符和内容 */
    printf("%s%s", prompt, buffer);
    
    /* 如果光标不在末尾，移动光标到正确位置 */
    int buffer_len = strlen(buffer);
    if (cursor_pos < buffer_len) {
        /* 计算光标应该在的位置 */
        int prompt_len = strlen(prompt);
        int desired_pos = prompt_len + cursor_pos;
        
        /* 使用绝对定位：\r移到行首，然后向右移动到目标位置 */
        printf("\r");
        if (desired_pos > 0) {
            printf("\033[%dC", desired_pos);
        }
    }
    
    fflush(stdout);
}

/*
 * 初始化输入状态
 */
void init_input_state(InputLineState *state) {
    if (!state) {
        return;
    }
    
    memset(state, 0, sizeof(InputLineState));
    state->pos = 0;
    state->len = 0;
    state->has_temp = false;
    state->state = INPUT_STATE_NORMAL;
    state->escape_pos = 0;
    state->raw_mode_enabled = false;
    state->prompt_shown = false;
    
    /* 获取终端大小 */
    get_terminal_size(&state->term_width, &state->term_height);
    
    /* 设置信号处理器 */
    setup_signal_handlers();
    
    /* 启用raw mode */
    enable_raw_mode(&state->original_termios);
    state->raw_mode_enabled = true;
}

/*
 * 清理输入状态
 */
void cleanup_input_state(InputLineState *state) {
    if (!state) {
        return;
    }
    
    if (state->raw_mode_enabled) {
        disable_raw_mode(&state->original_termios);
        state->raw_mode_enabled = false;
    }
}

/*
 * 处理Delete键（删除光标位置的字符）
 */
static void handle_delete_key(InputLineState *state, const char *prompt, 
                              CommandHistory *history) {
    if (state->pos < state->len) {
        /* 删除光标位置的字符 */
        memmove(&state->buffer[state->pos], &state->buffer[state->pos + 1], 
                state->len - state->pos);
        state->len--;
        state->buffer[state->len] = '\0';
        refresh_line(prompt, state->buffer, state->pos);
        reset_history_navigation(history);
    }
}

/*
 * 处理上箭头键
 */
static void handle_up_arrow(InputLineState *state, const char *prompt, 
                            CommandHistory *history) {
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
}

/*
 * 处理下箭头键
 */
static void handle_down_arrow(InputLineState *state, const char *prompt, 
                              CommandHistory *history) {
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
}

/*
 * 处理右箭头键
 */
static void handle_right_arrow(InputLineState *state) {
    if (state->pos < state->len) {
        state->pos++;
        printf("\033[C");
        fflush(stdout);
    }
}

/*
 * 处理左箭头键
 */
static void handle_left_arrow(InputLineState *state) {
    if (state->pos > 0) {
        state->pos--;
        printf("\033[D");
        fflush(stdout);
    }
}

/*
 * 处理Home键（光标移到行首）
 */
static void handle_home_key(InputLineState *state, const char *prompt) {
    if (state->pos > 0) {
        state->pos = 0;
        refresh_line(prompt, state->buffer, state->pos);
    }
}

/*
 * 处理End键（光标移到行尾）
 */
static void handle_end_key(InputLineState *state, const char *prompt) {
    if (state->pos < state->len) {
        state->pos = state->len;
        refresh_line(prompt, state->buffer, state->pos);
    }
}

/*
 * 处理Backspace键
 */
static void handle_backspace(InputLineState *state, const char *prompt, 
                             CommandHistory *history) {
    if (state->pos > 0) {
        /* 删除光标前的字符 */
        memmove(&state->buffer[state->pos - 1], &state->buffer[state->pos], 
                state->len - state->pos + 1);
        state->pos--;
        state->len--;
        refresh_line(prompt, state->buffer, state->pos);
        reset_history_navigation(history);
    }
}

/*
 * 处理可打印字符
 */
static void handle_printable_char(InputLineState *state, char c, const char *prompt, 
                                  CommandHistory *history) {
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
}

/*
 * FSM处理转义序列
 */
static int process_escape_sequence(InputLineState *state, char c, const char *prompt, 
                                   CommandHistory *history) {
    switch (state->state) {
        case INPUT_STATE_ESCAPE:
            if (c == '[') {
                state->state = INPUT_STATE_BRACKET;
                state->escape_seq[0] = '[';
                state->escape_pos = 1;
            } else if (c == 'O') {
                /* 某些终端的Home/End键发送ESC O H/F */
                state->state = INPUT_STATE_BRACKET;
                state->escape_seq[0] = 'O';
                state->escape_pos = 1;
            } else {
                /* 不是完整的转义序列，重置 */
                state->state = INPUT_STATE_NORMAL;
            }
            return 0;
            
        case INPUT_STATE_BRACKET:
            state->escape_seq[state->escape_pos++] = c;
            state->escape_seq[state->escape_pos] = '\0';
            
            /* 检查是否为完整的序列 */
            if (state->escape_seq[0] == '[') {
                /* ESC [ X 格式 */
                if (c == 'A') {
                    /* 上箭头 */
                    handle_up_arrow(state, prompt, history);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'B') {
                    /* 下箭头 */
                    handle_down_arrow(state, prompt, history);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'C') {
                    /* 右箭头 */
                    handle_right_arrow(state);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'D') {
                    /* 左箭头 */
                    handle_left_arrow(state);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'H') {
                    /* Home键 */
                    handle_home_key(state, prompt);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'F') {
                    /* End键 */
                    handle_end_key(state, prompt);
                    state->state = INPUT_STATE_NORMAL;
                } else if (isdigit(c)) {
                    /* ESC [ N 格式，需要等待更多字符 */
                    state->state = INPUT_STATE_BRACKET_PARAM;
                } else {
                    /* 未知序列，重置 */
                    state->state = INPUT_STATE_NORMAL;
                }
            } else if (state->escape_seq[0] == 'O') {
                /* ESC O X 格式 */
                if (c == 'H') {
                    /* Home键 */
                    handle_home_key(state, prompt);
                    state->state = INPUT_STATE_NORMAL;
                } else if (c == 'F') {
                    /* End键 */
                    handle_end_key(state, prompt);
                    state->state = INPUT_STATE_NORMAL;
                } else {
                    state->state = INPUT_STATE_NORMAL;
                }
            }
            return 0;
            
        case INPUT_STATE_BRACKET_PARAM:
            state->escape_seq[state->escape_pos++] = c;
            state->escape_seq[state->escape_pos] = '\0';
            
            if (c == '~') {
                /* ESC [ N ~ 格式 */
                if (strcmp(state->escape_seq, "[3~") == 0) {
                    /* Delete键 */
                    handle_delete_key(state, prompt, history);
                } else if (strcmp(state->escape_seq, "[1~") == 0 || 
                          strcmp(state->escape_seq, "[7~") == 0) {
                    /* Home键 */
                    handle_home_key(state, prompt);
                } else if (strcmp(state->escape_seq, "[4~") == 0 || 
                          strcmp(state->escape_seq, "[8~") == 0) {
                    /* End键 */
                    handle_end_key(state, prompt);
                }
                state->state = INPUT_STATE_NORMAL;
            } else if (!isdigit(c) && c != ';') {
                /* 未知序列，重置 */
                state->state = INPUT_STATE_NORMAL;
            }
            return 0;
            
        default:
            state->state = INPUT_STATE_NORMAL;
            return 0;
    }
}

/*
 * 非阻塞处理单个输入字符（服务器使用）
 */
int process_input_char(InputLineState *state, const char *prompt, 
                       CommandHistory *history, char *output, int output_size) {
    if (!state || !prompt || !history || !output) {
        return -1;
    }
    
    /* 处理窗口大小改变信号 */
    if (window_resized) {
        window_resized = 0;
        get_terminal_size(&state->term_width, &state->term_height);
        if (state->prompt_shown) {
            refresh_line(prompt, state->buffer, state->pos);
        }
    }
    
    /* 处理中断信号 */
    if (interrupted) {
        interrupted = 0;
        /* Ctrl+C - 取消当前输入 */
        printf("^C\n");
        memset(state->buffer, 0, sizeof(state->buffer));
        state->pos = 0;
        state->len = 0;
        state->has_temp = false;
        state->prompt_shown = false;
        state->state = INPUT_STATE_NORMAL;
        reset_history_navigation(history);
        return 0;
    }
    
    if (handlers_need_reset) {
        setup_signal_handlers();
        handlers_need_reset = 0;
    }
    
    if (raw_mode_reenable && !raw_mode_active) {
        enable_raw_mode(&state->original_termios);
        state->raw_mode_enabled = true;
        raw_mode_reenable = 0;
        if (state->prompt_shown) {
            refresh_line(prompt, state->buffer, state->pos);
        } else {
            printf("%s", prompt);
            fflush(stdout);
            state->prompt_shown = true;
            if (state->len > 0) {
                refresh_line(prompt, state->buffer, state->pos);
            }
        }
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
    
    /* 如果在转义序列中，继续处理 */
    if (state->state != INPUT_STATE_NORMAL) {
        return process_escape_sequence(state, c, prompt, history);
    }
    
    /* 正常字符处理 */
    if (c == 27) {  /* ESC */
        state->state = INPUT_STATE_ESCAPE;
        state->escape_pos = 0;
        memset(state->escape_seq, 0, sizeof(state->escape_seq));
        return 0;
    } else if (c == 127 || c == 8) {  /* Backspace */
        handle_backspace(state, prompt, history);
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
        state->state = INPUT_STATE_NORMAL;
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
        state->state = INPUT_STATE_NORMAL;
        reset_history_navigation(history);
        return 0;
    } else if (c == 26) {  /* Ctrl+Z */
        printf("^Z\n");
        fflush(stdout);
        state->raw_mode_enabled = false;
        state->prompt_shown = false;
        state->state = INPUT_STATE_NORMAL;
        handle_sigtstp(SIGTSTP);
        return 0;
    } else if (isprint(c)) {
        /* 可打印字符 */
        handle_printable_char(state, c, prompt, history);
        return 0;
    }
    
    return 0;
}

/*
 * 读取一行输入，支持历史导航（客户端使用）
 */
int read_line_with_history(char *buffer, int buffer_size, const char *prompt, 
                           CommandHistory *history, int socket_fd) {
    if (!buffer || buffer_size <= 0 || !prompt || !history) {
        return -1;
    }
    
    struct termios original_termios;
    enable_raw_mode(&original_termios);
    
    /* 设置信号处理器 */
    setup_signal_handlers();
    
    memset(buffer, 0, buffer_size);
    int pos = 0;           /* 当前光标位置 */
    int len = 0;           /* 当前输入长度 */
    char temp_buffer[MAX_COMMAND_LENGTH];  /* 临时保存用户正在输入的内容 */
    memset(temp_buffer, 0, sizeof(temp_buffer));
    bool has_temp = false;
    
    InputState state = INPUT_STATE_NORMAL;
    char escape_seq[8];
    int escape_pos = 0;
    
    /* 显示初始提示符 */
    printf("%s", prompt);
    fflush(stdout);
    
    while (1) {
        /* 处理窗口大小改变信号 */
        if (window_resized) {
            window_resized = 0;
            refresh_line(prompt, buffer, pos);
        }
        
        /* 处理中断信号 */
        if (interrupted) {
            interrupted = 0;
            printf("\n");
            disable_raw_mode(&original_termios);
            return -1;
        }
        
        if (handlers_need_reset) {
            setup_signal_handlers();
            handlers_need_reset = 0;
        }
        
        if (raw_mode_reenable && !raw_mode_active) {
            enable_raw_mode(&original_termios);
            raw_mode_reenable = 0;
            refresh_line(prompt, buffer, pos);
        }
        
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
        tv.tv_usec = 50000;  /* 50ms超时，用于响应信号 */
        
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
        
        /* FSM处理转义序列 */
        if (state == INPUT_STATE_ESCAPE) {
            if (c == '[') {
                state = INPUT_STATE_BRACKET;
                escape_seq[0] = '[';
                escape_pos = 1;
            } else if (c == 'O') {
                state = INPUT_STATE_BRACKET;
                escape_seq[0] = 'O';
                escape_pos = 1;
            } else {
                state = INPUT_STATE_NORMAL;
            }
            continue;
        } else if (state == INPUT_STATE_BRACKET) {
            escape_seq[escape_pos++] = c;
            escape_seq[escape_pos] = '\0';
            
            if (escape_seq[0] == '[') {
                if (c == 'A') {
                    /* 上箭头 */
                    const char *prev_cmd = get_previous_command(history);
                    if (prev_cmd) {
                        if (!has_temp && len > 0) {
                            strncpy(temp_buffer, buffer, sizeof(temp_buffer) - 1);
                            has_temp = true;
                        }
                        strncpy(buffer, prev_cmd, buffer_size - 1);
                        buffer[buffer_size - 1] = '\0';
                        len = strlen(buffer);
                        pos = len;
                        refresh_line(prompt, buffer, pos);
                    }
                    state = INPUT_STATE_NORMAL;
                } else if (c == 'B') {
                    /* 下箭头 */
                    if (history->navigating) {
                        const char *next_cmd = get_next_command(history);
                        if (next_cmd) {
                            if (next_cmd[0] == '\0' && has_temp) {
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
                    state = INPUT_STATE_NORMAL;
                } else if (c == 'C') {
                    /* 右箭头 */
                    if (pos < len) {
                        pos++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                    state = INPUT_STATE_NORMAL;
                } else if (c == 'D') {
                    /* 左箭头 */
                    if (pos > 0) {
                        pos--;
                        printf("\033[D");
                        fflush(stdout);
                    }
                    state = INPUT_STATE_NORMAL;
                } else if (c == 'H') {
                    /* Home键 */
                    pos = 0;
                    refresh_line(prompt, buffer, pos);
                    state = INPUT_STATE_NORMAL;
                } else if (c == 'F') {
                    /* End键 */
                    pos = len;
                    refresh_line(prompt, buffer, pos);
                    state = INPUT_STATE_NORMAL;
                } else if (isdigit(c)) {
                    state = INPUT_STATE_BRACKET_PARAM;
                } else {
                    state = INPUT_STATE_NORMAL;
                }
            } else if (escape_seq[0] == 'O') {
                if (c == 'H') {
                    /* Home键 */
                    pos = 0;
                    refresh_line(prompt, buffer, pos);
                } else if (c == 'F') {
                    /* End键 */
                    pos = len;
                    refresh_line(prompt, buffer, pos);
                }
                state = INPUT_STATE_NORMAL;
            }
            continue;
        } else if (state == INPUT_STATE_BRACKET_PARAM) {
            escape_seq[escape_pos++] = c;
            escape_seq[escape_pos] = '\0';
            
            if (c == '~') {
                if (strcmp(escape_seq, "[3~") == 0) {
                    /* Delete键 */
                    if (pos < len) {
                        memmove(&buffer[pos], &buffer[pos + 1], len - pos);
                        len--;
                        buffer[len] = '\0';
                        refresh_line(prompt, buffer, pos);
                        reset_history_navigation(history);
                    }
                } else if (strcmp(escape_seq, "[1~") == 0 || 
                          strcmp(escape_seq, "[7~") == 0) {
                    /* Home键 */
                    pos = 0;
                    refresh_line(prompt, buffer, pos);
                } else if (strcmp(escape_seq, "[4~") == 0 || 
                          strcmp(escape_seq, "[8~") == 0) {
                    /* End键 */
                    pos = len;
                    refresh_line(prompt, buffer, pos);
                }
                state = INPUT_STATE_NORMAL;
            } else if (!isdigit(c) && c != ';') {
                state = INPUT_STATE_NORMAL;
            }
            continue;
        }
        
        /* 正常字符处理 */
        if (c == 27) {  /* ESC */
            state = INPUT_STATE_ESCAPE;
            escape_pos = 0;
            memset(escape_seq, 0, sizeof(escape_seq));
        } else if (c == 127 || c == 8) {  /* Backspace */
            if (pos > 0) {
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
            buffer[len] = '\0';
            reset_history_navigation(history);
            return len;
        } else if (c == 4) {  /* Ctrl+D */
            if (len == 0) {
                printf("\n");
                disable_raw_mode(&original_termios);
                return -1;
            }
        } else if (c == 3) {  /* Ctrl+C */
            printf("\n");
            disable_raw_mode(&original_termios);
            return -1;
        } else if (c == 26) {  /* Ctrl+Z */
            printf("^Z\n");
            fflush(stdout);
            state = INPUT_STATE_NORMAL;
            handle_sigtstp(SIGTSTP);
            continue;
        } else if (isprint(c)) {
            /* 可打印字符 */
            if (len < buffer_size - 1) {
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
