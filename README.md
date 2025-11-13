# Multi-Client TCP Server and Client

A high-performance multi-client TCP server and client implementation in C for Ubuntu/Linux systems.

## Features

- **Multi-Client Server**: Supports up to 128 concurrent client connections
- **Epoll-Based Multiplexing**: Efficient event-driven I/O using Linux epoll for high throughput
- **Non-blocking Sockets**: Optimized for handling multiple clients with minimal overhead
- **Interactive Client**: Full-duplex communication with echo server responses
- **Graceful Shutdown**: Proper signal handling and resource cleanup
- **Clear Error Handling**: Informative error messages and logging

## Requirements

- Ubuntu/Linux system with GCC compiler
- POSIX-compliant C library
- Linux kernel support for epoll (all modern Linux systems)

## Building

### Compile all programs:
```bash
make
```

### Compile individual programs:
```bash
make server      # Compile server only
make client      # Compile client only
```

### Clean up compiled files:
```bash
make clean
```

## Running

### Start the server:
```bash
./server <port>
```

Example:
```bash
./server 8888
```

The server will display:
```
[Server] Listening on port 8888 (Max clients: 128)
```

### Connect a client:
```bash
./client <server_ip> <server_port>
```

Example (localhost):
```bash
./client 127.0.0.1 8888
```

Example (remote server):
```bash
./client 192.168.1.100 8888
```

### Testing with multiple clients:

Terminal 1 - Start server:
```bash
./server 8888
```

Terminal 2 - First client:
```bash
./client 127.0.0.1 8888
```

Terminal 3 - Second client:
```bash
./client 127.0.0.1 8888
```

Terminal 4 - Add more clients as needed:
```bash
./client 127.0.0.1 8888
```

### Interactive client commands:

Once connected, type messages to send to the server:

```
[You] Hello Server
[Server] Echo: Hello Server
[You] This is a test message
[Server] Echo: This is a test message
[You] quit
[Client] Disconnecting...
```

Type `quit` to disconnect from the server.

## Architecture

### Common Header (`common.h`)
- Shared constants and includes
- MAX_CLIENTS: 128 concurrent connections
- BUFFER_SIZE: 4096 bytes per message
- LISTEN_BACKLOG: 128 pending connections

### Server (`server.c`)
- **Epoll-based event loop**: Efficiently handles all client connections
- **Non-blocking I/O**: Prevents blocking on any single client
- **Dynamic client management**: Accepts and removes clients on-demand
- **Echo protocol**: Echoes received messages back to clients
- **Connection logging**: Displays all connection/disconnection events

Key functions:
- `set_nonblocking()`: Converts socket to non-blocking mode
- `cleanup()`: Signal handler for graceful shutdown
- `main()`: Event loop and client handling

### Client (`client.c`)
- **Standard socket connection**: Connects to specified server and port
- **Interactive CLI**: Read-write loop for sending/receiving messages
- **Input validation**: Checks IP addresses and port numbers
- **Graceful disconnection**: Handles server closure and user quit command

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

## File Structure

```
.
├── common.h       # Shared constants and includes
├── server.c       # Multi-client TCP server implementation
├── client.c       # TCP client implementation
├── Makefile       # Build configuration
└── README.md      # This file
```

## License

This is a demonstration project for educational purposes.

## Author

Implemented for multi-client TCP communication requirements.
