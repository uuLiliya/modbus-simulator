#!/bin/bash

# 测试server.c使用socket fd作为客户端标识

PORT=15555
SERVER_LOG=test_server.log
CLIENT1_LOG=test_client1.log
CLIENT2_LOG=test_client2.log

echo "启动服务器..."
./build/server $PORT > $SERVER_LOG 2>&1 &
SERVER_PID=$!
sleep 1

echo "连接客户端1..."
(sleep 0.5; echo "Hello from client 1"; sleep 0.5; echo "quit") | ./build/client 127.0.0.1 $PORT > $CLIENT1_LOG 2>&1 &
CLIENT1_PID=$!
sleep 1

echo "连接客户端2..."
(sleep 0.5; echo "Hello from client 2"; sleep 0.5; echo "quit") | ./build/client 127.0.0.1 $PORT > $CLIENT2_LOG 2>&1 &
CLIENT2_PID=$!
sleep 2

# 等待客户端结束
wait $CLIENT1_PID 2>/dev/null
wait $CLIENT2_PID 2>/dev/null

# 关闭服务器
kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "=== 服务器日志 ==="
cat $SERVER_LOG
echo ""

echo "=== 客户端1日志 ==="
cat $CLIENT1_LOG
echo ""

echo "=== 客户端2日志 ==="
cat $CLIENT2_LOG
echo ""

# 验证结果
echo "=== 验证结果 ==="

# 检查服务器日志中是否使用fd作为标识
if grep -q '\[fd:[0-9]\+\]' $SERVER_LOG; then
    echo "✓ 服务器使用 [fd:X] 格式标识客户端"
else
    echo "✗ 服务器未使用 [fd:X] 格式"
fi

# 检查是否有"客户端已连接"的消息
if grep -q '\[fd:[0-9]\+\] 客户端已连接' $SERVER_LOG; then
    echo "✓ 连接日志格式正确"
else
    echo "✗ 连接日志格式错误"
fi

# 检查回显消息是否包含fd
if grep -q '\[服务器回显\]\[fd:[0-9]\+\]' $CLIENT1_LOG || grep -q '\[服务器回显\]\[fd:[0-9]\+\]' $CLIENT2_LOG; then
    echo "✓ 回显消息使用 fd 标识"
else
    echo "✗ 回显消息未使用 fd 标识"
fi

# 检查欢迎消息
if grep -q '文件描述符为' $CLIENT1_LOG && grep -q '文件描述符为' $CLIENT2_LOG; then
    echo "✓ 欢迎消息包含文件描述符"
else
    echo "✗ 欢迎消息格式错误"
fi

# 清理
rm -f $SERVER_LOG $CLIENT1_LOG $CLIENT2_LOG

echo ""
echo "测试完成！"
