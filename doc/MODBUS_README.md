# Modbus TCP 协议支持

本项目在原有TCP服务器/客户端的基础上添加了完整的 Modbus TCP 协议支持。

## 功能特性

### 支持的功能码

1. **FC03 (0x03) - 读保持寄存器 (Read Holding Registers)**
   - 读取一个或多个保持寄存器的值
   - 最多支持一次读取125个寄存器
   
2. **FC06 (0x06) - 写单个寄存器 (Write Single Register)**
   - 向指定地址写入单个寄存器值
   - 支持0-65535的寄存器地址范围

### 寄存器配置

- **保持寄存器 (Holding Registers)**: 1000个 (地址0-999)
  - 可读写
  - 初始值为地址值本身 (地址0=0, 地址1=1, ...)
  
- **输入寄存器 (Input Registers)**: 1000个 (地址0-999)
  - 只读（当前实现）
  - 初始值为 1000 + 地址值

## Modbus TCP 帧格式

### MBAP Header（7字节）

```
字节0-1: Transaction Identifier (事务标识符，大端序)
字节2-3: Protocol Identifier (协议标识符，固定为0x0000)
字节4-5: Length (后续字节数，大端序)
字节6:   Unit Identifier (单元标识符)
```

### FC03 读保持寄存器

**请求格式：**
```
MBAP Header (7字节)
功能码 0x03 (1字节)
起始地址 (2字节，大端序)
寄存器数量 (2字节，大端序)
```

**响应格式：**
```
MBAP Header (7字节)
功能码 0x03 (1字节)
字节计数 (1字节) = 寄存器数量 * 2
寄存器值 (N*2字节，每个寄存器2字节，大端序)
```

### FC06 写单个寄存器

**请求格式：**
```
MBAP Header (7字节)
功能码 0x06 (1字节)
寄存器地址 (2字节，大端序)
寄存器值 (2字节，大端序)
```

**响应格式（回显请求）：**
```
MBAP Header (7字节)
功能码 0x06 (1字节)
寄存器地址 (2字节，大端序)
寄存器值 (2字节，大端序)
```

### 错误响应

**格式：**
```
MBAP Header (7字节)
错误功能码 (1字节) = 原功能码 | 0x80
异常码 (1字节)
```

**异常码：**
- 0x01: 非法功能码
- 0x02: 非法数据地址
- 0x03: 非法数据值
- 0x04: 服务器设备故障

## 使用说明

### 编译

```bash
make clean
make
```

### 启动服务器

```bash
./build/server <端口号>
```

例如：
```bash
./build/server 502    # Modbus TCP 标准端口
./build/server 10502  # 自定义端口
```

### 客户端命令

启动客户端：
```bash
./build/client <服务器IP> <端口号>
```

#### Modbus 命令

1. **读取保持寄存器：**
   ```
   modbus read <起始地址> <数量>
   ```
   
   示例：
   ```
   modbus read 100 5      # 读取地址100开始的5个寄存器
   modbus read 0 10       # 读取地址0开始的10个寄存器
   ```

2. **写单个寄存器：**
   ```
   modbus write <地址> <值>
   ```
   
   示例：
   ```
   modbus write 100 1234   # 将地址100的寄存器设为1234
   modbus write 50 9999    # 将地址50的寄存器设为9999
   ```

3. **普通文本消息：**
   - 直接输入任意文本，服务器会以回显方式返回
   - 用于测试非Modbus通信

4. **退出：**
   ```
   quit
   ```

## 测试示例

### 自动化测试

运行自动测试脚本：
```bash
./tests/test_modbus_interactive.sh
```

该脚本会自动测试：
1. FC03 读取寄存器
2. FC06 写入寄存器
3. 验证写入结果
4. 普通文本消息

### 手动测试

启动服务器：
```bash
./build/server 10502 &
```

测试FC03读取：
```bash
(sleep 0.5; echo "modbus read 100 5"; sleep 2; echo "quit") | ./build/client 127.0.0.1 10502
```

预期输出：
```
[客户端] Modbus 响应：事务ID=1, 功能码=0x03, 单元ID=1
[客户端] FC03 读取成功，共 5 个寄存器：
  寄存器[0] = 100 (0x0064)
  寄存器[1] = 101 (0x0065)
  寄存器[2] = 102 (0x0066)
  寄存器[3] = 103 (0x0067)
  寄存器[4] = 104 (0x0068)
```

测试FC06写入：
```bash
(sleep 0.5; echo "modbus write 100 9999"; sleep 2; echo "quit") | ./build/client 127.0.0.1 10502
```

预期输出：
```
[客户端] Modbus 响应：事务ID=1, 功能码=0x06, 单元ID=1
[客户端] FC06 写入成功：寄存器[100] = 9999 (0x270F)
```

验证写入：
```bash
(sleep 0.5; echo "modbus read 100 1"; sleep 2; echo "quit") | ./build/client 127.0.0.1 10502
```

预期输出：
```
[客户端] FC03 读取成功，共 1 个寄存器：
  寄存器[0] = 9999 (0x270F)
```

## 代码结构

### 文件列表

- `include/modbus.h` - Modbus协议常量、结构体和函数接口定义
- `src/modbus.c` - Modbus协议编码/解码实现
- `src/server.c` - TCP服务器，支持Modbus请求处理
- `src/client.c` - TCP客户端，支持发送Modbus请求
- `include/common.h` - 公共头文件
- `Makefile` - 编译配置

### 主要数据结构

```c
// MBAP Header结构体
typedef struct {
    uint16_t transaction_id;    // 事务标识符
    uint16_t protocol_id;       // 协议标识符（0x0000）
    uint16_t length;            // 后续字节数
    uint8_t unit_id;            // 单元标识符
} ModbusMBAPHeader;

// PDU结构体
typedef struct {
    uint8_t function_code;      // 功能码
    uint8_t data[252];          // 功能数据
    size_t data_length;         // 数据长度
} ModbusPDU;

// 完整消息结构体
typedef struct {
    ModbusMBAPHeader mbap;      // MBAP Header
    ModbusPDU pdu;              // PDU
} ModbusTCPMessage;
```

### 主要函数

**modbus.c:**
- `modbus_parse_request()` - 解析Modbus请求
- `modbus_build_fc03_request()` - 构建FC03请求
- `modbus_build_fc03_response()` - 构建FC03响应
- `modbus_build_fc06_request()` - 构建FC06请求
- `modbus_build_fc06_response()` - 构建FC06响应
- `modbus_build_error_response()` - 构建错误响应
- `modbus_parse_fc03_response()` - 解析FC03响应
- `modbus_get_exception_string()` - 获取异常码描述

**server.c:**
- `init_modbus_registers()` - 初始化寄存器数组
- `is_modbus_message()` - 检测是否为Modbus消息
- `handle_modbus_request()` - 处理Modbus请求

**client.c:**
- `send_modbus_read_request()` - 发送FC03读请求
- `send_modbus_write_request()` - 发送FC06写请求
- `handle_modbus_response()` - 处理Modbus响应
- `handle_user_command()` - 处理用户命令

## 技术细节

### 协议兼容性

- 完全符合Modbus TCP标准（Modbus Application Protocol V1.1b3）
- 所有多字节数值使用大端序（Big-Endian）
- 支持标准的异常响应机制

### 并发支持

- 服务器使用epoll实现高性能并发
- 支持最多128个客户端同时连接
- 每个客户端的Modbus请求独立处理

### 协议检测

- 服务器自动检测Modbus TCP消息（通过MBAP Header的协议标识符）
- 非Modbus消息按普通文本处理（回显协议）
- 客户端自动识别Modbus响应和文本消息

## 注意事项

1. **寄存器范围：** 当前实现支持1000个寄存器（地址0-999），超出范围会返回异常码0x02
2. **事务ID：** 客户端自动管理事务ID，每次请求递增
3. **单元ID：** 客户端默认使用单元ID = 0x01
4. **错误处理：** 所有非法请求都会返回适当的异常响应
5. **混合通信：** 同一连接可以混合使用Modbus协议和普通文本消息

## 扩展建议

如需扩展功能，可以考虑：

1. 添加FC04（读输入寄存器）支持
2. 添加FC10（写多个寄存器）支持
3. 增加寄存器数量
4. 添加寄存器持久化（保存到文件）
5. 实现更复杂的寄存器映射（如模拟真实设备）
6. 添加Modbus异常统计和日志

## 许可证

本项目遵循原项目的许可证。
