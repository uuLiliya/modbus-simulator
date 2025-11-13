#!/bin/bash

# 测试服务器命令功能（使用fd标识）- 简化版

PORT=15558

echo "启动服务器..."
./server $PORT > /tmp/server_test.log 2>&1 &
SERVER_PID=$!
sleep 1

echo "启动客户端1..."
(sleep 5) | ./client 127.0.0.1 $PORT > /tmp/client1_test.log 2>&1 &
CLIENT1_PID=$!
sleep 1

echo "启动客户端2..."
(sleep 5) | ./client 127.0.0.1 $PORT > /tmp/client2_test.log 2>&1 &
CLIENT2_PID=$!
sleep 2

# 检查服务器日志
echo ""
echo "=== 检查服务器日志 ==="
cat /tmp/server_test.log
echo ""

# 关闭所有进程
kill $CLIENT1_PID $CLIENT2_PID 2>/dev/null
sleep 0.5
kill -SIGINT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "=== 验证结果 ==="

# 检查连接消息
if grep -q '\[fd:[0-9]\+\] 客户端已连接' /tmp/server_test.log; then
    echo "✓ 连接消息使用 [fd:X] 格式"
    # 显示所有fd
    echo "  检测到的fd:"
    grep '\[fd:[0-9]\+\] 客户端已连接' /tmp/server_test.log | grep -o '\[fd:[0-9]\+\]'
else
    echo "✗ 连接消息格式错误"
fi

# 检查欢迎消息
echo ""
echo "=== 客户端1收到的欢迎消息 ==="
grep "文件描述符为" /tmp/client1_test.log
echo ""
echo "=== 客户端2收到的欢迎消息 ==="
grep "文件描述符为" /tmp/client2_test.log

# 清理
rm -f /tmp/server_test.log /tmp/client1_test.log /tmp/client2_test.log

echo ""
echo "测试完成！"
