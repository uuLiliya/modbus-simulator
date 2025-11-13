/*
 * Modbus TCP 协议实现
 * 
 * 实现 Modbus TCP 协议的编码和解码功能
 */

#include "modbus.h"
#include <string.h>
#include <arpa/inet.h>

/* ============= 辅助函数 ============= */

/*
 * 从缓冲区读取大端序16位整数
 */
static uint16_t read_uint16_be(const uint8_t *buffer) {
    return (uint16_t)(buffer[0] << 8) | buffer[1];
}

/*
 * 将16位整数以大端序写入缓冲区
 */
static void write_uint16_be(uint8_t *buffer, uint16_t value) {
    buffer[0] = (value >> 8) & 0xFF;
    buffer[1] = value & 0xFF;
}

/* ============= MBAP Header 处理函数 ============= */

/*
 * 解析 MBAP Header
 * 
 * MBAP Header 格式（7字节）：
 * 字节0-1：Transaction ID（事务标识符，大端序）
 * 字节2-3：Protocol ID（协议标识符，大端序，固定0x0000）
 * 字节4-5：Length（长度，大端序，表示后续字节数）
 * 字节6：Unit ID（单元标识符）
 */
static bool parse_mbap_header(const uint8_t *buffer, size_t length, ModbusMBAPHeader *header) {
    /* 检查长度是否足够 */
    if (length < MODBUS_MBAP_HEADER_LENGTH) {
        return false;
    }

    /* 解析各字段（大端序） */
    header->transaction_id = read_uint16_be(&buffer[0]);
    header->protocol_id = read_uint16_be(&buffer[2]);
    header->length = read_uint16_be(&buffer[4]);
    header->unit_id = buffer[6];

    /* 验证协议标识符必须为0x0000 */
    if (header->protocol_id != MODBUS_PROTOCOL_ID) {
        return false;
    }

    /* 验证长度字段（至少要有 Unit ID + Function Code = 2字节） */
    if (header->length < 2) {
        return false;
    }

    return true;
}

/*
 * 构建 MBAP Header
 */
static void build_mbap_header(uint8_t *buffer, uint16_t transaction_id,
                             uint16_t length, uint8_t unit_id) {
    write_uint16_be(&buffer[0], transaction_id);        /* 字节0-1：事务ID */
    write_uint16_be(&buffer[2], MODBUS_PROTOCOL_ID);    /* 字节2-3：协议ID（0x0000） */
    write_uint16_be(&buffer[4], length);                /* 字节4-5：后续长度 */
    buffer[6] = unit_id;                                /* 字节6：单元ID */
}

/* ============= 请求解析函数 ============= */

/*
 * 解析 Modbus TCP 请求消息
 */
bool modbus_parse_request(const uint8_t *buffer, size_t length, ModbusTCPMessage *message) {
    if (!buffer || !message || length < MODBUS_MBAP_HEADER_LENGTH + 1) {
        return false;
    }

    /* 解析 MBAP Header */
    if (!parse_mbap_header(buffer, length, &message->mbap)) {
        return false;
    }

    /* 验证消息总长度是否匹配 */
    size_t expected_length = MODBUS_MBAP_HEADER_LENGTH + message->mbap.length - 1;
    if (length < expected_length) {
        return false;
    }

    /* 解析 PDU */
    size_t pdu_length = message->mbap.length - 1; /* 减去 Unit ID */
    if (pdu_length > MODBUS_MAX_PDU_LENGTH) {
        return false;
    }

    message->pdu.function_code = buffer[MODBUS_MBAP_HEADER_LENGTH];
    message->pdu.data_length = pdu_length - 1; /* 减去 Function Code */

    if (message->pdu.data_length > 0) {
        memcpy(message->pdu.data,
               &buffer[MODBUS_MBAP_HEADER_LENGTH + 1],
               message->pdu.data_length);
    }

    return true;
}

/* ============= FC03 读保持寄存器 ============= */

/*
 * 构建 FC03 读保持寄存器响应
 * 
 * 响应格式：
 * MBAP Header（7字节）
 * 功能码 0x03（1字节）
 * 字节计数（1字节）= 寄存器数量 * 2
 * 寄存器值（N*2字节，每个寄存器2字节，大端序）
 */
size_t modbus_build_fc03_response(uint16_t transaction_id, uint8_t unit_id,
                                  const uint16_t *registers, uint16_t quantity,
                                  uint8_t *buffer, size_t buffer_size) {
    if (!registers || !buffer || quantity == 0 || quantity > MODBUS_MAX_READ_REGISTERS) {
        return 0;
    }

    uint8_t byte_count = quantity * 2;
    size_t total_length = MODBUS_MBAP_HEADER_LENGTH + 1 + 1 + byte_count;

    if (buffer_size < total_length) {
        return 0;
    }

    /* 构建 MBAP Header */
    /* Length = Unit ID(1) + Function Code(1) + Byte Count(1) + Data(N*2) */
    uint16_t mbap_length = 1 + 1 + 1 + byte_count;
    build_mbap_header(buffer, transaction_id, mbap_length, unit_id);

    /* 构建 PDU */
    size_t offset = MODBUS_MBAP_HEADER_LENGTH;
    buffer[offset++] = MODBUS_FC_READ_HOLDING_REGISTERS;  /* 功能码 */
    buffer[offset++] = byte_count;                        /* 字节计数 */

    /* 写入寄存器值（大端序） */
    for (uint16_t i = 0; i < quantity; i++) {
        write_uint16_be(&buffer[offset], registers[i]);
        offset += 2;
    }

    return total_length;
}

/*
 * 构建 FC03 读保持寄存器请求
 * 
 * 请求格式：
 * MBAP Header（7字节）
 * 功能码 0x03（1字节）
 * 起始地址（2字节，大端序）
 * 寄存器数量（2字节，大端序）
 */
size_t modbus_build_fc03_request(uint16_t transaction_id, uint8_t unit_id,
                                 uint16_t start_address, uint16_t quantity,
                                 uint8_t *buffer, size_t buffer_size) {
    if (!buffer || quantity == 0 || quantity > MODBUS_MAX_READ_REGISTERS) {
        return 0;
    }

    size_t total_length = MODBUS_MBAP_HEADER_LENGTH + 1 + 2 + 2;

    if (buffer_size < total_length) {
        return 0;
    }

    /* 构建 MBAP Header */
    /* Length = Unit ID(1) + Function Code(1) + Start Address(2) + Quantity(2) = 6 */
    build_mbap_header(buffer, transaction_id, 6, unit_id);

    /* 构建 PDU */
    size_t offset = MODBUS_MBAP_HEADER_LENGTH;
    buffer[offset++] = MODBUS_FC_READ_HOLDING_REGISTERS;  /* 功能码 */
    write_uint16_be(&buffer[offset], start_address);      /* 起始地址 */
    offset += 2;
    write_uint16_be(&buffer[offset], quantity);           /* 寄存器数量 */
    offset += 2;

    return total_length;
}

/*
 * 解析 FC03 读保持寄存器响应
 */
uint16_t modbus_parse_fc03_response(const ModbusTCPMessage *message,
                                    uint16_t *registers, uint16_t max_registers) {
    if (!message || !registers || max_registers == 0) {
        return 0;
    }

    /* 验证功能码 */
    if (message->pdu.function_code != MODBUS_FC_READ_HOLDING_REGISTERS) {
        return 0;
    }

    /* 验证数据长度至少有字节计数字段 */
    if (message->pdu.data_length < 1) {
        return 0;
    }

    uint8_t byte_count = message->pdu.data[0];
    uint16_t register_count = byte_count / 2;

    /* 验证字节计数 */
    if (byte_count % 2 != 0 || register_count > max_registers) {
        return 0;
    }

    /* 验证数据长度 */
    if (message->pdu.data_length < (size_t)(1 + byte_count)) {
        return 0;
    }

    /* 解析寄存器值 */
    for (uint16_t i = 0; i < register_count; i++) {
        registers[i] = read_uint16_be(&message->pdu.data[1 + i * 2]);
    }

    return register_count;
}

/* ============= FC06 写单个寄存器 ============= */

/*
 * 构建 FC06 写单个寄存器响应
 * 
 * 响应格式（与请求相同）：
 * MBAP Header（7字节）
 * 功能码 0x06（1字节）
 * 寄存器地址（2字节，大端序）
 * 寄存器值（2字节，大端序）
 */
size_t modbus_build_fc06_response(uint16_t transaction_id, uint8_t unit_id,
                                  uint16_t register_address, uint16_t register_value,
                                  uint8_t *buffer, size_t buffer_size) {
    if (!buffer) {
        return 0;
    }

    size_t total_length = MODBUS_MBAP_HEADER_LENGTH + 1 + 2 + 2;

    if (buffer_size < total_length) {
        return 0;
    }

    /* 构建 MBAP Header */
    /* Length = Unit ID(1) + Function Code(1) + Address(2) + Value(2) = 6 */
    build_mbap_header(buffer, transaction_id, 6, unit_id);

    /* 构建 PDU */
    size_t offset = MODBUS_MBAP_HEADER_LENGTH;
    buffer[offset++] = MODBUS_FC_WRITE_SINGLE_REGISTER;  /* 功能码 */
    write_uint16_be(&buffer[offset], register_address);  /* 寄存器地址 */
    offset += 2;
    write_uint16_be(&buffer[offset], register_value);    /* 寄存器值 */
    offset += 2;

    return total_length;
}

/*
 * 构建 FC06 写单个寄存器请求
 * 
 * 请求格式：
 * MBAP Header（7字节）
 * 功能码 0x06（1字节）
 * 寄存器地址（2字节，大端序）
 * 寄存器值（2字节，大端序）
 */
size_t modbus_build_fc06_request(uint16_t transaction_id, uint8_t unit_id,
                                 uint16_t register_address, uint16_t register_value,
                                 uint8_t *buffer, size_t buffer_size) {
    /* FC06 请求和响应格式完全相同 */
    return modbus_build_fc06_response(transaction_id, unit_id,
                                     register_address, register_value,
                                     buffer, buffer_size);
}

/* ============= 错误响应 ============= */

/*
 * 构建 Modbus 错误响应
 * 
 * 错误响应格式：
 * MBAP Header（7字节）
 * 功能码 | 0x80（1字节）
 * 异常码（1字节）
 */
size_t modbus_build_error_response(uint16_t transaction_id, uint8_t unit_id,
                                   uint8_t function_code, uint8_t exception_code,
                                   uint8_t *buffer, size_t buffer_size) {
    if (!buffer) {
        return 0;
    }

    size_t total_length = MODBUS_MBAP_HEADER_LENGTH + 1 + 1;

    if (buffer_size < total_length) {
        return 0;
    }

    /* 构建 MBAP Header */
    /* Length = Unit ID(1) + Error Function Code(1) + Exception Code(1) = 3 */
    build_mbap_header(buffer, transaction_id, 3, unit_id);

    /* 构建错误 PDU */
    size_t offset = MODBUS_MBAP_HEADER_LENGTH;
    buffer[offset++] = function_code | MODBUS_FC_ERROR;  /* 功能码 | 0x80 */
    buffer[offset++] = exception_code;                   /* 异常码 */

    return total_length;
}

/* ============= 辅助函数 ============= */

/*
 * 获取异常码的描述文本
 */
const char* modbus_get_exception_string(uint8_t exception_code) {
    switch (exception_code) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
            return "非法功能码";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
            return "非法数据地址";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
            return "非法数据值";
        case MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE:
            return "服务器设备故障";
        default:
            return "未知异常";
    }
}
