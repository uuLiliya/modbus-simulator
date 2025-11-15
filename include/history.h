#ifndef HISTORY_H
#define HISTORY_H

/*
 * 命令历史管理模块 - 按Linux readline标准实现
 * 
 * 功能：
 * - 有限状态机处理Escape序列
 * - 循环队列历史管理（最多100条命令）
 * - 非阻塞I/O（使用select()）
 * - 完整的终端控制（raw mode）
 * - 行编辑功能（backspace, delete, 箭头键）
 * - 信号处理（SIGWINCH, SIGINT, SIGTSTP)
 */

#include "common.h"
#include <signal.h>

/* 信号处理相关 */
extern volatile sig_atomic_t window_resized;
extern volatile sig_atomic_t interrupted;

/* 初始化和清理 */
void init_history(CommandHistory *history);
void cleanup_history(CommandHistory *history);

/* 历史管理 */
void add_to_history(CommandHistory *history, const char *command);
const char* get_previous_command(CommandHistory *history);
const char* get_next_command(CommandHistory *history);
void reset_history_navigation(CommandHistory *history);

/* 终端raw mode控制 */
void enable_raw_mode(struct termios *original);
void disable_raw_mode(struct termios *original);

/* 交互式输入 - 客户端使用 */
int read_line_with_history(char *buffer, int buffer_size, const char *prompt, 
                           CommandHistory *history, int socket_fd);

/* 非阻塞输入 - 服务器使用 */
void init_input_state(InputLineState *state);
void cleanup_input_state(InputLineState *state);
int process_input_char(InputLineState *state, const char *prompt, 
                       CommandHistory *history, char *output, int output_size);

/* 信号处理器 */
void setup_signal_handlers(void);
void handle_sigwinch(int sig);
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void handle_sigcont(int sig);

/* 行刷新和显示 */
void refresh_line(const char *prompt, const char *buffer, int cursor_pos);
void clear_line(void);
void move_cursor_to_pos(int current_pos, int target_pos);

/* 向后兼容的旧接口（服务器使用） */
typedef InputLineState ServerInputState;
#define init_server_input init_input_state
#define cleanup_server_input cleanup_input_state
#define process_server_input_char process_input_char

#endif
