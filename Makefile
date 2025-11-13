CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGETS = server client

.PHONY: all clean help

all: $(TARGETS)

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f $(TARGETS)
	rm -f *.o

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
