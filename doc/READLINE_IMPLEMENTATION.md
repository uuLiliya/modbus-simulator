# 按Linux readline标准实现命令历史

## 概述

本项目按照Linux readline库和bash的标准实现了完整的命令历史导航功能，提供与bash相同的用户体验。

## 核心设计

### 1. 有限状态机(FSM)处理Escape序列

使用严格的状态机处理终端escape序列，防止不完整序列导致异常：

```c
typedef enum {
    INPUT_STATE_NORMAL = 0,      /* 正常字符输入 */
    INPUT_STATE_ESCAPE,          /* 收到ESC (0x1B) */
    INPUT_STATE_BRACKET,         /* 收到ESC[ */
    INPUT_STATE_BRACKET_PARAM    /* 收到ESC[N（N为参数） */
} InputState;
```

**支持的Escape序列：**
- `\033[A` - 上箭头（历史上一条）
- `\033[B` - 下箭头（历史下一条）
- `\033[C` - 右箭头（光标右移）
- `\033[D` - 左箭头（光标左移）
- `\033[3~` - Delete键（删除光标处字符）
- `\033[H` / `\033[1~` / `\033[7~` - Home键（光标移到行首）
- `\033[F` / `\033[4~` / `\033[8~` - End键（光标移到行尾）
- `\033OH` / `\033OF` - 某些终端的Home/End键

### 2. 循环队列历史管理

实现高效的循环缓冲区，存储最近100条命令：

```c
typedef struct {
    char commands[MAX_HISTORY_SIZE][MAX_COMMAND_LENGTH];
    int count;          /* 当前已存储的历史命令总数 */
    int head;           /* 循环缓冲区头部位置（最新命令） */
    int current;        /* 当前浏览位置（用于上下导航） */
    bool navigating;    /* 是否正在浏览历史记录 */
} CommandHistory;
```

**特性：**
- O(1)时间复杂度的添加和查询
- 自动去重：连续相同命令不重复存储
- 空命令过滤
- 循环覆盖最旧的命令

### 3. 非阻塞I/O（使用select()）

使用`select()`系统调用实现多路复用，支持同时监听：
- 标准输入（STDIN_FILENO）
- 网络socket（客户端模式）

```c
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(STDIN_FILENO, &read_fds);
if (socket_fd >= 0) {
    FD_SET(socket_fd, &read_fds);
}

struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 }; /* 50ms超时 */
int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
```

**优势：**
- 不阻塞其他事件处理
- 支持信号响应
- 适合服务器和客户端环境

### 4. 完整的终端控制

#### Raw Mode设置

禁用标准终端处理，实现自定义行编辑：

```c
struct termios raw;
tcgetattr(STDIN_FILENO, &raw);

/* 禁用规范模式、回显和信号处理 */
raw.c_lflag &= ~(ICANON | ECHO | ISIG);

/* 禁用特殊字符处理 */
raw.c_iflag &= ~(IXON | ICRNL);

/* 禁用输出处理 */
raw.c_oflag &= ~(OPOST);

/* 设置非阻塞读取 */
raw.c_cc[VMIN] = 0;
raw.c_cc[VTIME] = 0;

tcsetattr(STDIN_FILENO, TCSANOW, &raw);
```

#### ANSI Escape码控制

使用标准ANSI escape序列控制光标和显示：

- `\r` - 回车（移到行首）
- `\033[K` - 清除从光标到行尾
- `\033[nC` - 光标向右移动n个字符
- `\033[nD` - 光标向左移动n个字符

### 5. 行编辑功能

完整的行编辑支持，与bash行为一致：

- **Backspace/Delete**: 删除字符（光标前/光标处）
- **左右箭头**: 移动光标
- **Home/End**: 移到行首/行尾
- **插入模式**: 在光标位置插入字符
- **历史编辑**: 可在历史命令基础上继续编辑
- **临时保存**: 导航历史时保存当前未提交的输入

### 6. 信号处理

完整的POSIX信号支持，确保优雅退出和终端恢复：

```c
void setup_signal_handlers(void) {
    struct sigaction sa;
    
    /* SIGWINCH - 窗口大小改变 */
    sa.sa_handler = handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
    
    /* SIGINT - Ctrl+C */
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    
    /* SIGTSTP - Ctrl+Z */
    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);
    
    /* SIGCONT - 继续运行 */
    sa.sa_handler = handle_sigcont;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCONT, &sa, NULL);
}
```

**信号处理特性：**
- 窗口改变时自动刷新显示
- Ctrl+C优雅退出，恢复终端模式
- Ctrl+Z暂停进程，恢复规范模式
- 进程恢复后重新启用raw mode

## 文件结构

```
include/
├── common.h          # 共享数据结构（CommandHistory, InputLineState）
└── history.h         # 历史管理公共接口

src/
├── history.c         # 核心实现（1069行）
├── client.c          # 客户端集成
└── server.c          # 服务器集成
```

## API接口

### 历史管理

```c
/* 初始化和清理 */
void init_history(CommandHistory *history);
void cleanup_history(CommandHistory *history);

/* 添加和查询 */
void add_to_history(CommandHistory *history, const char *command);
const char* get_previous_command(CommandHistory *history);
const char* get_next_command(CommandHistory *history);
void reset_history_navigation(CommandHistory *history);
```

### 终端控制

```c
/* Raw mode */
void enable_raw_mode(struct termios *original);
void disable_raw_mode(struct termios *original);

/* 显示刷新 */
void refresh_line(const char *prompt, const char *buffer, int cursor_pos);
void clear_line(void);
void move_cursor_to_pos(int current_pos, int target_pos);
```

### 输入处理

```c
/* 客户端使用（阻塞式） */
int read_line_with_history(char *buffer, int buffer_size, const char *prompt, 
                           CommandHistory *history, int socket_fd);

/* 服务器使用（非阻塞式） */
void init_input_state(InputLineState *state);
void cleanup_input_state(InputLineState *state);
int process_input_char(InputLineState *state, const char *prompt, 
                       CommandHistory *history, char *output, int output_size);
```

## 使用示例

### 客户端集成

```c
#include "history.h"

CommandHistory cmd_history;
init_history(&cmd_history);

char buffer[BUFFER_SIZE];
while (1) {
    int result = read_line_with_history(buffer, BUFFER_SIZE, 
                                       "[你] ", &cmd_history, socket_fd);
    
    if (result < 0) break;  /* 用户退出 */
    if (result == 0) continue;  /* 空行 */
    if (result == -2) {  /* Socket有数据 */
        /* 处理服务器消息 */
        continue;
    }
    
    /* 添加到历史 */
    add_to_history(&cmd_history, buffer);
    
    /* 处理用户命令 */
    handle_command(buffer);
}
```

### 服务器集成

```c
#include "history.h"

CommandHistory cmd_history;
InputLineState input_state;

init_history(&cmd_history);
init_input_state(&input_state);

char output[MAX_COMMAND_LENGTH];
while (1) {
    int result = process_input_char(&input_state, "[服务器] ", 
                                    &cmd_history, output, sizeof(output));
    
    if (result == 1) {
        /* 完成一行输入 */
        add_to_history(&cmd_history, output);
        handle_command(output);
    } else if (result < 0) {
        /* 退出 */
        break;
    }
}

cleanup_input_state(&input_state);
```

## 键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| ↑ | 显示上一条历史命令 |
| ↓ | 显示下一条历史命令 |
| ← | 光标左移 |
| → | 光标右移 |
| Home | 光标移到行首 |
| End | 光标移到行尾 |
| Backspace | 删除光标前的字符 |
| Delete | 删除光标处的字符 |
| Ctrl+C | 取消当前输入/退出 |
| Ctrl+D | EOF（空行时退出） |
| Ctrl+Z | 暂停进程 |
| 可打印字符 | 在光标位置插入 |

## 验证标准

完成的实现满足以下验证标准：

✅ 输入命令后按上箭头显示上一条  
✅ 多次上箭头可逐条往前翻  
✅ 按下箭头返回新的命令  
✅ 可编辑历史命令再发送  
✅ backspace/delete正常删除  
✅ 左右箭头正确移动光标  
✅ Home/End键移动到行首/尾  
✅ Ctrl+C优雅退出且恢复终端  
✅ Ctrl+Z暂停并恢复  
✅ 窗口改变自动刷新  
✅ 编译无警告  
✅ 功能稳定可靠  

## 编译

```bash
# 调试模式（默认）
make

# 纯数据模式
make DEBUG_MODE=0

# 清理并重新编译
make clean && make
```

## 兼容性

- **标准**: 遵循POSIX.1-2008标准
- **终端**: 支持所有VT100/ANSI兼容终端
- **系统**: Linux, macOS, *BSD
- **编译器**: GCC, Clang (使用-std=c99)

## 性能特性

- **时间复杂度**: 
  - 添加命令: O(1)
  - 查询历史: O(1)
  - 渲染行: O(n)，n为行长度
  
- **空间复杂度**: O(100 * 1024) = 100KB（固定）

- **非阻塞**: 不阻塞网络I/O或其他事件

## 实现参考

本实现遵循以下标准和最佳实践：

1. **readline库**: GNU readline的行为模式
2. **bash**: Bash shell的历史导航逻辑
3. **POSIX**: 标准终端控制和信号处理
4. **VT100**: ANSI escape序列标准

## 作者注释

此实现是一个完整的、生产级的readline-compatible行编辑器，可以直接用于：
- 交互式shell
- REPL环境
- 网络客户端
- 服务器管理控制台
- 任何需要命令历史的交互式应用

代码质量：
- 无编译警告
- 信号处理安全
- 内存管理正确
- 错误处理完整
- 终端状态恢复可靠
