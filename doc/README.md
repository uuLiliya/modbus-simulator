# Multi-Client TCP Server and Client with Modbus TCP Support

A high-performance multi-client TCP server and client implementation in C for Ubuntu/Linux systems, featuring full Modbus TCP protocol support.

## Features

### Core Features
- **Multi-Client Server**: Supports up to 128 concurrent client connections
- **Client Identification**: Each client identified by socket file descriptor
- **Server-Initiated Messaging**: Server can send messages to specific clients or broadcast to all
- **Epoll-Based Multiplexing**: Efficient event-driven I/O using Linux epoll for high throughput
- **Non-blocking Sockets**: Optimized for handling multiple clients with minimal overhead
- **Interactive Client**: Full-duplex communication with real-time message receiving
- **Server Commands**: Interactive server commands for client management and messaging
- **Command History Navigation**: Use arrow keys to browse command history (up to 100 commands)
- **Graceful Shutdown**: Proper signal handling and resource cleanup
- **Clear Error Handling**: Informative error messages and logging

### Modbus TCP Features
- **FC03 Support**: Read Holding Registers (up to 125 registers per request)
- **FC06 Support**: Write Single Register
- **Standard Compliant**: Fully compliant with Modbus TCP specification
- **Error Handling**: Complete exception response mechanism
- **Mixed Protocol**: Supports both Modbus and plain text communication on same connection
- **1000 Registers**: Pre-configured with 1000 holding registers and 1000 input registers
- **Real-time Display**: Shows register values and changes in both server and client

## Requirements

- Ubuntu/Linux system with GCC compiler
- POSIX-compliant C library
- Linux kernel support for epoll (all modern Linux systems)

## Project Structure

The project follows a standard directory layout:

```
.
├── src/                         # Source code files
│   ├── server.c                 # Multi-client TCP server implementation
│   ├── client.c                 # TCP client implementation
│   ├── modbus.c                 # Modbus protocol implementation
│   └── history.c                # Command history management
├── include/                     # Header files
│   ├── common.h                 # Shared constants and structures
│   └── modbus.h                 # Modbus protocol definitions
├── build/                       # Compiled binaries (generated)
│   ├── server                   # Server executable
│   └── client                   # Client executable
├── doc/                         # Documentation
│   ├── README.md                # Main documentation (this file)
│   ├── MODBUS_README.md         # Modbus TCP detailed guide
│   ├── HISTORY_README.md        # Command history features
│   ├── TESTING.md               # Testing documentation
│   ├── IMPLEMENTATION_SUMMARY.md # Implementation details
│   └── GIT_GUIDE.md             # Git workflow reference
├── tests/                       # Test scripts
│   ├── test_modbus_interactive.sh
│   ├── test_history.sh
│   ├── test_history_auto.sh
│   └── verify_compilation.sh
├── Makefile                     # Build configuration
└── .gitignore                   # Git ignore rules
```

## Building

### Compile all programs:
```bash
make
```

This will create the executables in the `build/` directory.

### Clean up compiled files:
```bash
make clean
```

## Running

### Start the server:
```bash
./build/server <port>
```

Example:
```bash
./build/server 8888
```

The server will display messages similar to:
```
[服务器] 正在监听端口 8888（最大客户端数: 128）
[服务器] 输入 'help' 查看可用命令（支持上下箭头键导航命令历史）
```

**Note**: The server supports command history navigation with arrow keys - press UP to view previous commands, DOWN to move forward in history.

### Connect a client:
```bash
./build/client <server_ip> <server_port>
```

Example (localhost):
```bash
./build/client 127.0.0.1 8888
```

Example (remote server):
```bash
./build/client 192.168.1.100 8888
```

### Testing with multiple clients:

Terminal 1 - Start server:
```bash
./build/server 8888
```

Terminal 2 - First client:
```bash
./build/client 127.0.0.1 8888
```

Terminal 3 - Second client:
```bash
./build/client 127.0.0.1 8888
```

Terminal 4 - Add more clients as needed:
```bash
./build/client 127.0.0.1 8888
```

### Server Commands:

Once the server is running, you can use these interactive commands:

- **`list`** - Display all connected clients with their IDs and addresses
- **`send <client_id> <message>`** - Send a message to a specific client (e.g., `send Client_1 Hello`)
- **`broadcast <message>`** - Send a message to all connected clients
- **`help`** - Display help information for available commands

Example server session:
```
[服务器] 正在监听端口 8888（最大客户端数: 128）
[服务器] 输入 'help' 查看可用命令

list
[服务器] 当前连接的客户端列表：
  - Client_1 (fd=5, 地址=127.0.0.1:54321)
  - Client_2 (fd=6, 地址=127.0.0.1:54322)
[服务器] 总计：2 个客户端

send Client_1 你好客户端1！
[服务器] 已向 Client_1 发送消息: 你好客户端1！

broadcast 服务器公告：系统将在5分钟后重启
[服务器] 已广播消息给 2 个客户端
```

### Interactive Client Usage:

Once connected, type messages to send to the server. The client can receive messages at any time and supports command history navigation:

```
[客户端] 正在连接 127.0.0.1:8888...
[客户端] 连接成功！
[客户端] 可用命令：
  modbus read <起始地址> <数量>   - 读取保持寄存器 (FC03)
  modbus write <地址> <值>        - 写单个寄存器 (FC06)
  quit                             - 退出程序
  或输入任意文本消息发送给服务器
  使用上下箭头键导航命令历史

[服务器消息] [服务器通知] 欢迎，您的文件描述符为 5。

[你] 你好服务器
[服务器消息] [服务器回显][fd:5] 你好服务器

[服务器消息] [服务器] 你好客户端！

[你] quit
[客户端] 正在断开连接...
```

**Command History Features**:
- Press **UP arrow** to browse previous commands
- Press **DOWN arrow** to move forward in history
- Press **LEFT/RIGHT arrows** to move cursor within the line
- Press **Backspace** to edit commands
- Commands can be edited after recalling from history

Type `quit` to disconnect from the server.

## Architecture

### Common Header (`common.h`)
- Shared constants and includes
- `ClientInfo` structure storing file descriptor, unique ID, address, and active state
- MAX_CLIENTS: 128 concurrent connections
- BUFFER_SIZE: 4096 bytes per message
- LISTEN_BACKLOG: 128 pending connections

### Server (`server.c`)
- **Epoll-based event loop**: Efficiently handles all client connections
- **Non-blocking I/O**: Prevents blocking on any single client
- **Unique client identification**: Assigns `Client_X` style IDs and logs per-client activity
- **Server-side messaging**: Interactive commands for targeted messages and broadcasts
- **Echo protocol**: Echoes received messages back to sending clients
- **Robust logging**: Tracks connections, disconnections, and message origins
- **Graceful fallback**: Continues to operate even when stdin command mode is unavailable

Key functions:
- `set_nonblocking()`: Converts socket to non-blocking mode
- `handle_stdin_input()`: Parses and executes server console commands
- `disconnect_client()`: Cleans up epoll state and closes client connections
- `main()`: Event loop and client handling

### Client (`client.c`)
- **Standard socket connection**: Connects to specified server and port
- **Select-based multiplexing**: Simultaneously monitors stdin and socket for real-time updates
- **Interactive CLI**: Sends user input and displays server messages with clear prefixes
- **Graceful disconnection**: Handles server closure and user quit command
- **Responsive UI**: Automatically reprints prompts after incoming server messages

## Performance Characteristics

- **Throughput**: Handles high message rates with minimal latency
- **Concurrency**: Efficiently manages 128 concurrent connections
- **Memory**: Minimal per-connection overhead (~64 bytes per client)
- **CPU**: Event-driven design minimizes CPU usage

## Connection Limits

The server is configured to handle:
- Maximum concurrent clients: 128
- Maximum pending connections: 128
- Buffer size per message: 4096 bytes
- Event queue size: 128 events

## Error Handling

The application handles:
- Invalid port numbers (1-65535)
- Invalid IP addresses
- Socket creation failures
- Bind/listen errors
- Client disconnections (graceful and abnormal)
- I/O errors during read/write operations
- Epoll errors

## Troubleshooting

### Port already in use:
```
Error: bind: Address already in use
```
Solution: Wait a few seconds for the OS to release the port, or use a different port number.

### Connection refused:
```
Error: connect: Connection refused
```
Solution: Ensure the server is running on the specified IP and port.

### Invalid IP address:
```
Error: Invalid server IP address.
```
Solution: Use a valid IPv4 address (e.g., 127.0.0.1, 192.168.1.1).

### Too many open files:
This occurs when file descriptor limit is exceeded (unlikely with 128 client limit but possible on low-resource systems).
Solution: Increase system file descriptor limit with `ulimit -n`.

## Code Quality

- **Clear comments**: Each section is documented
- **Error checking**: All system calls check for errors
- **Resource cleanup**: Proper handling of file descriptors
- **Signal handling**: Graceful shutdown on SIGINT/SIGTERM
- **Standard compliance**: POSIX-compliant C99 code

## Modbus TCP Quick Start

### Basic Usage

Start server:
```bash
./build/server 502    # Standard Modbus TCP port
```

Connect client and send Modbus commands:
```bash
./build/client 127.0.0.1 502
modbus read 100 5        # Read 5 registers starting from address 100
modbus write 100 9999    # Write value 9999 to register at address 100
quit
```

### Run Tests

```bash
./tests/test_modbus_interactive.sh
```

For detailed Modbus TCP documentation, see **[MODBUS_README.md](MODBUS_README.md)**.

## Related Documentation

- **[Modbus TCP Documentation](MODBUS_README.md)** - Complete Modbus TCP protocol implementation guide
- **[Command History Documentation](HISTORY_README.md)** - Detailed guide for command history navigation features
- **[Testing Guide](TESTING.md)** - Comprehensive testing documentation
- **[Git Command Guide](GIT_GUIDE.md)** - Comprehensive Git commands reference including basic setup, branching, merging, and common workflows

## License

This is a demonstration project for educational purposes.

## Author

Implemented for multi-client TCP communication requirements.
