# 定义C语言编译器为gcc
CC = gcc
# 定义编译标志：开启所有警告、额外警告、使用C99标准、开启优化，添加头文件搜索路径
# DEBUG_MODE 编译选项：1=调试模式（默认），0=纯数据流模式
DEBUG_MODE ?= 1
CFLAGS = -Wall -Wextra -std=c99 -O2 -Iinclude -DDEBUG_MODE=$(DEBUG_MODE)
# 定义源文件目录
SRC_DIR = src
# 定义头文件目录
INCLUDE_DIR = include
# 定义构建输出目录
BUILD_DIR = build
# 定义要编译的目标程序：服务器和客户端（输出到build目录）
TARGETS = $(BUILD_DIR)/server $(BUILD_DIR)/client

# 声明伪目标，这些目标不是实际的文件，避免与同名文件冲突
.PHONY: all clean help

# 默认目标：编译所有程序（服务器和客户端）
all: $(TARGETS)

# 编译服务器程序：依赖server.c、modbus.c、history.c、common.h和modbus.h文件
# 使用gcc编译器，按照CFLAGS标志，将server.c、modbus.c和history.c编译成名为server的可执行文件
$(BUILD_DIR)/server: $(SRC_DIR)/server.c $(SRC_DIR)/modbus.c $(SRC_DIR)/history.c $(INCLUDE_DIR)/common.h $(INCLUDE_DIR)/modbus.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/server $(SRC_DIR)/server.c $(SRC_DIR)/modbus.c $(SRC_DIR)/history.c

# 编译客户端程序：依赖client.c、modbus.c、history.c、common.h和modbus.h文件
# 使用gcc编译器，按照CFLAGS标志，将client.c、modbus.c和history.c编译成名为client的可执行文件
$(BUILD_DIR)/client: $(SRC_DIR)/client.c $(SRC_DIR)/modbus.c $(SRC_DIR)/history.c $(INCLUDE_DIR)/common.h $(INCLUDE_DIR)/modbus.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/client $(SRC_DIR)/client.c $(SRC_DIR)/modbus.c $(SRC_DIR)/history.c

# 创建build目录（如果不存在）
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 清理目标：删除所有编译生成的文件
# 强制删除build目录及其所有内容
clean:
	rm -rf $(BUILD_DIR)

# 帮助目标：显示所有可用的make目标和使用说明
help:
	@echo "Available targets:"
	@echo "  all      - Build all programs (server and client)"
	@echo "  clean    - Remove all built files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  DEBUG_MODE=1 (default) - Build in debug mode with all debug info"
	@echo "  DEBUG_MODE=0           - Build in pure data mode (compatible with standard Modbus tools)"
	@echo ""
	@echo "Project structure:"
	@echo "  src/       - Source code files (.c)"
	@echo "  include/   - Header files (.h)"
	@echo "  build/     - Compiled binaries and object files"
	@echo "  doc/       - Documentation files"
	@echo "  tests/     - Test scripts"
	@echo ""
	@echo "Usage:"
	@echo "  Debug mode (default):   make"
	@echo "  Pure data mode:         make DEBUG_MODE=0"
	@echo "  Start server: ./build/server <port>"
	@echo "  Start client: ./build/client <server_ip> <server_port>"
	@echo "  Example: ./build/server 8888 &"
	@echo "  Example: ./build/client 127.0.0.1 8888"
