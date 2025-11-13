#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Modbus TCP 协议头文件
 * 
 * 定义 Modbus TCP 协议的常量、数据结构和函数接口
 */

/* ============= Modbus TCP 协议常量 ============= */

/* MBAP Header 长度（7字节） */
#define MODBUS_MBAP_HEADER_LENGTH 7

/* PDU 最大长度（253字节） */
#define MODBUS_MAX_PDU_LENGTH 253

/* 完整 Modbus TCP 消息最大长度（MBAP + PDU = 260字节） */
#define MODBUS_MAX_MESSAGE_LENGTH (MODBUS_MBAP_HEADER_LENGTH + MODBUS_MAX_PDU_LENGTH)

/* Modbus 协议标识符（固定为0x0000） */
#define MODBUS_PROTOCOL_ID 0x0000

/* 寄存器相关常量 */
#define MODBUS_MAX_REGISTERS 65536      /* 寄存器最大地址空间（0-65535） */
#define MODBUS_MAX_READ_REGISTERS 125   /* 一次读取的最大寄存器数量 */
#define MODBUS_MAX_WRITE_REGISTERS 123  /* 一次写入的最大寄存器数量 */

/* ============= Modbus 功能码 ============= */

/* FC03：读保持寄存器（Read Holding Registers） */
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03

/* FC04：读输入寄存器（Read Input Registers） */
#define MODBUS_FC_READ_INPUT_REGISTERS 0x04

/* FC06：写单个寄存器（Write Single Register） */
#define MODBUS_FC_WRITE_SINGLE_REGISTER 0x06

/* FC10：写多个寄存器（Write Multiple Registers） */
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

/* 错误响应标志（功能码最高位置1） */
#define MODBUS_FC_ERROR 0x80

/* ============= Modbus 异常码 ============= */

/* 异常码01：非法功能码 */
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01

/* 异常码02：非法数据地址 */
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02

/* 异常码03：非法数据值 */
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03

/* 异常码04：服务器设备故障 */
#define MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE 0x04

/* ============= Modbus TCP MBAP Header 结构体 ============= */

/*
 * MBAP Header（Modbus Application Protocol Header）
 * 共7个字节，结构如下：
 * 
 * 字节0-1：Transaction Identifier（事务标识符，2字节，大端序）
 *          - 用于匹配请求和响应
 *          - 客户端设置，服务器原样返回
 * 
 * 字节2-3：Protocol Identifier（协议标识符，2字节，固定为0x0000）
 *          - Modbus 协议固定为 0x0000
 * 
 * 字节4-5：Length（长度字段，2字节，大端序）
 *          - 表示后续字节数（Unit ID + PDU）
 *          - 不包括 MBAP Header 前6个字节
 * 
 * 字节6：Unit Identifier（单元标识符，1字节）
 *        - 用于路由到特定设备
 *        - TCP 直接连接通常使用 0xFF 或 0x00
 */
typedef struct {
    uint16_t transaction_id;    /* 字节0-1：事务标识符 */
    uint16_t protocol_id;       /* 字节2-3：协议标识符（固定0x0000） */
    uint16_t length;            /* 字节4-5：后续字节数（Unit ID + PDU长度） */
    uint8_t unit_id;            /* 字节6：单元标识符 */
} ModbusMBAPHeader;

/* ============= Modbus PDU 结构体 ============= */

/*
 * PDU（Protocol Data Unit，协议数据单元）
 * 
 * 字节7：Function Code（功能码，1字节）
 *        - 定义执行的操作类型
 *        - 0x03：读保持寄存器
 *        - 0x06：写单个寄存器
 *        - 0x80+功能码：错误响应
 * 
 * 字节8+：Data（数据，可变长度）
 *         - 根据功能码不同，数据格式不同
 */
typedef struct {
    uint8_t function_code;                      /* 功能码 */
    uint8_t data[MODBUS_MAX_PDU_LENGTH - 1];    /* 功能数据（最大252字节） */
    size_t data_length;                         /* 实际数据长度 */
} ModbusPDU;

/* ============= 完整的 Modbus TCP 消息结构体 ============= */

/*
 * 完整的 Modbus TCP 消息
 * 由 MBAP Header + PDU 组成
 */
typedef struct {
    ModbusMBAPHeader mbap;  /* MBAP Header（7字节） */
    ModbusPDU pdu;          /* PDU（可变长度） */
} ModbusTCPMessage;

/* ============= FC03 读保持寄存器 请求/响应 结构 ============= */

/*
 * FC03 读保持寄存器 - 请求格式
 * 
 * PDU 数据部分（5字节）：
 * 字节0：功能码 0x03
 * 字节1-2：起始地址（2字节，大端序）
 * 字节3-4：寄存器数量（2字节，大端序，1-125）
 */
typedef struct {
    uint16_t start_address;     /* 起始地址 */
    uint16_t quantity;          /* 寄存器数量 */
} ModbusFC03Request;

/*
 * FC03 读保持寄存器 - 响应格式
 * 
 * PDU 数据部分（可变长度）：
 * 字节0：功能码 0x03
 * 字节1：字节计数（N = 寄存器数量 * 2）
 * 字节2+：寄存器值（每个寄存器2字节，大端序）
 */
typedef struct {
    uint8_t byte_count;                         /* 字节计数 */
    uint16_t registers[MODBUS_MAX_READ_REGISTERS]; /* 寄存器值数组 */
} ModbusFC03Response;

/* ============= FC06 写单个寄存器 请求/响应 结构 ============= */

/*
 * FC06 写单个寄存器 - 请求格式
 * 
 * PDU 数据部分（5字节）：
 * 字节0：功能码 0x06
 * 字节1-2：寄存器地址（2字节，大端序）
 * 字节3-4：寄存器值（2字节，大端序）
 */
typedef struct {
    uint16_t register_address;  /* 寄存器地址 */
    uint16_t register_value;    /* 寄存器值 */
} ModbusFC06Request;

/*
 * FC06 写单个寄存器 - 响应格式
 * 
 * 响应与请求完全相同（回显）：
 * 字节0：功能码 0x06
 * 字节1-2：寄存器地址（2字节，大端序）
 * 字节3-4：寄存器值（2字节，大端序）
 */
typedef ModbusFC06Request ModbusFC06Response;

/* ============= Modbus 错误响应结构 ============= */

/*
 * Modbus 错误响应格式
 * 
 * PDU 数据部分（2字节）：
 * 字节0：功能码 | 0x80（错误标志）
 * 字节1：异常码
 */
typedef struct {
    uint8_t function_code;  /* 功能码 | 0x80 */
    uint8_t exception_code; /* 异常码 */
} ModbusErrorResponse;

/* ============= 函数接口声明 ============= */

/*
 * 解析 Modbus TCP 请求消息
 * 
 * 参数：
 *   buffer - 接收到的原始字节数据
 *   length - 数据长度
 *   message - 输出：解析后的 Modbus 消息结构
 * 
 * 返回：
 *   成功返回 true，失败返回 false
 */
bool modbus_parse_request(const uint8_t *buffer, size_t length, ModbusTCPMessage *message);

/*
 * 构建 FC03 读保持寄存器响应
 * 
 * 参数：
 *   transaction_id - 事务标识符（来自请求）
 *   unit_id - 单元标识符（来自请求）
 *   registers - 寄存器值数组
 *   quantity - 寄存器数量
 *   buffer - 输出：响应消息缓冲区
 *   buffer_size - 缓冲区大小
 * 
 * 返回：
 *   响应消息的实际长度（字节数）
 */
size_t modbus_build_fc03_response(uint16_t transaction_id, uint8_t unit_id,
                                  const uint16_t *registers, uint16_t quantity,
                                  uint8_t *buffer, size_t buffer_size);

/*
 * 构建 FC06 写单个寄存器响应
 * 
 * 参数：
 *   transaction_id - 事务标识符（来自请求）
 *   unit_id - 单元标识符（来自请求）
 *   register_address - 寄存器地址
 *   register_value - 寄存器值
 *   buffer - 输出：响应消息缓冲区
 *   buffer_size - 缓冲区大小
 * 
 * 返回：
 *   响应消息的实际长度（字节数）
 */
size_t modbus_build_fc06_response(uint16_t transaction_id, uint8_t unit_id,
                                  uint16_t register_address, uint16_t register_value,
                                  uint8_t *buffer, size_t buffer_size);

/*
 * 构建 Modbus 错误响应
 * 
 * 参数：
 *   transaction_id - 事务标识符（来自请求）
 *   unit_id - 单元标识符（来自请求）
 *   function_code - 原始功能码
 *   exception_code - 异常码
 *   buffer - 输出：响应消息缓冲区
 *   buffer_size - 缓冲区大小
 * 
 * 返回：
 *   响应消息的实际长度（字节数）
 */
size_t modbus_build_error_response(uint16_t transaction_id, uint8_t unit_id,
                                   uint8_t function_code, uint8_t exception_code,
                                   uint8_t *buffer, size_t buffer_size);

/*
 * 构建 FC03 读保持寄存器请求
 * 
 * 参数：
 *   transaction_id - 事务标识符
 *   unit_id - 单元标识符
 *   start_address - 起始地址
 *   quantity - 寄存器数量
 *   buffer - 输出：请求消息缓冲区
 *   buffer_size - 缓冲区大小
 * 
 * 返回：
 *   请求消息的实际长度（字节数）
 */
size_t modbus_build_fc03_request(uint16_t transaction_id, uint8_t unit_id,
                                 uint16_t start_address, uint16_t quantity,
                                 uint8_t *buffer, size_t buffer_size);

/*
 * 构建 FC06 写单个寄存器请求
 * 
 * 参数：
 *   transaction_id - 事务标识符
 *   unit_id - 单元标识符
 *   register_address - 寄存器地址
 *   register_value - 寄存器值
 *   buffer - 输出：请求消息缓冲区
 *   buffer_size - 缓冲区大小
 * 
 * 返回：
 *   请求消息的实际长度（字节数）
 */
size_t modbus_build_fc06_request(uint16_t transaction_id, uint8_t unit_id,
                                 uint16_t register_address, uint16_t register_value,
                                 uint8_t *buffer, size_t buffer_size);

/*
 * 解析 FC03 读保持寄存器响应
 * 
 * 参数：
 *   message - Modbus 消息结构
 *   registers - 输出：寄存器值数组
 *   max_registers - 数组最大容量
 * 
 * 返回：
 *   实际读取的寄存器数量，失败返回 0
 */
uint16_t modbus_parse_fc03_response(const ModbusTCPMessage *message,
                                    uint16_t *registers, uint16_t max_registers);

/*
 * 获取异常码的描述文本
 * 
 * 参数：
 *   exception_code - 异常码
 * 
 * 返回：
 *   异常码的中文描述
 */
const char* modbus_get_exception_string(uint8_t exception_code);

#endif /* MODBUS_H */
