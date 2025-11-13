#!/bin/bash
# 启动服务器（在后台）
echo "启动服务器..."
./build/server 10502 &
SERVER_PID=$!
sleep 2

echo ""
echo "=== 测试 1: FC03 读保持寄存器 ==="
(
  sleep 0.5
  echo "modbus read 100 5"
  sleep 2
  echo "quit"
) | ./build/client 127.0.0.1 10502

echo ""
echo "=== 测试 2: FC06 写单个寄存器 ==="
(
  sleep 0.5
  echo "modbus write 100 9999"
  sleep 2
  echo "quit"
) | ./build/client 127.0.0.1 10502

echo ""
echo "=== 测试 3: 验证写入（再次读取） ==="
(
  sleep 0.5
  echo "modbus read 100 1"
  sleep 2
  echo "quit"
) | ./build/client 127.0.0.1 10502

echo ""
echo "=== 测试 4: 普通文本消息 ==="
(
  sleep 0.5
  echo "Hello from client!"
  sleep 2
  echo "quit"
) | ./build/client 127.0.0.1 10502

echo ""
echo "停止服务器..."
kill -INT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "测试完成！"
