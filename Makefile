# 定义C语言编译器为gcc
CC = gcc
# 定义编译标志：开启所有警告、额外警告、使用C99标准、开启优化
CFLAGS = -Wall -Wextra -std=c99 -O2
# 定义要编译的目标程序：服务器和客户端
TARGETS = server client

# 声明伪目标，这些目标不是实际的文件，避免与同名文件冲突
.PHONY: all clean help

# 默认目标：编译所有程序（服务器和客户端）
all: $(TARGETS)

# 编译服务器程序：依赖server.c和common.h文件
# 使用gcc编译器，按照CFLAGS标志，将server.c编译成名为server的可执行文件
server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

# 编译客户端程序：依赖client.c和common.h文件
# 使用gcc编译器，按照CFLAGS标志，将client.c编译成名为client的可执行文件
client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

# 清理目标：删除所有编译生成的文件
# 强制删除所有目标可执行文件（server和client）和目标文件（.o文件）
clean:
	rm -f $(TARGETS)
	rm -f *.o

# 帮助目标：显示所有可用的make目标和使用说明
help:
	@echo "Available targets:"
	@echo "  all      - Build all programs (server and client)"
	@echo "  server   - Build server executable"
	@echo "  client   - Build client executable"
	@echo "  clean    - Remove all built files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  Start server: ./server <port>"
	@echo "  Start client: ./client <server_ip> <server_port>"
	@echo "  Example: ./server 8888 &"
	@echo "  Example: ./client 127.0.0.1 8888"