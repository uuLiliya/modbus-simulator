#!/bin/bash
# Modbus TCP 功能测试脚本

echo "=== Modbus TCP 功能测试 ==="
echo ""

# 测试1：读取保持寄存器（FC03）
echo "测试1：读取保持寄存器（地址100-104，共5个）"
echo "modbus read 100 5" | timeout 3 ./client 127.0.0.1 9502
echo ""
sleep 1

# 测试2：写单个寄存器（FC06）
echo "测试2：写单个寄存器（地址100，值9999）"
echo "modbus write 100 9999" | timeout 3 ./client 127.0.0.1 9502
echo ""
sleep 1

# 测试3：再次读取刚才写入的寄存器，验证写入成功
echo "测试3：验证写入结果（读取地址100）"
echo "modbus read 100 1" | timeout 3 ./client 127.0.0.1 9502
echo ""
sleep 1

# 测试4：读取多个寄存器
echo "测试4：读取更多寄存器（地址0-9，共10个）"
echo "modbus read 0 10" | timeout 3 ./client 127.0.0.1 9502
echo ""
sleep 1

# 测试5：普通文本消息（非Modbus）
echo "测试5：发送普通文本消息"
echo "Hello Server!" | timeout 3 ./client 127.0.0.1 9502
echo ""

echo "=== 测试完成 ==="
echo ""
echo "查看服务器日志："
cat server_modbus.log
