#!/bin/bash

# 测试服务器命令功能（使用fd标识）

PORT=15557
SERVER_FIFO=/tmp/server_input_$$
CLIENT1_LOG=test_cmd_client1.log
CLIENT2_LOG=test_cmd_client2.log
SERVER_LOG=test_cmd_server.log

# 清理函数
cleanup() {
    rm -f $SERVER_FIFO $CLIENT1_LOG $CLIENT2_LOG $SERVER_LOG
    pkill -P $$ 2>/dev/null
}

trap cleanup EXIT

# 创建命名管道用于服务器输入
mkfifo $SERVER_FIFO

echo "启动服务器..."
./build/server $PORT < $SERVER_FIFO > $SERVER_LOG 2>&1 &
SERVER_PID=$!
exec 3>$SERVER_FIFO  # 保持管道打开
sleep 1

echo "启动客户端1（长连接）..."
(sleep 100) | ./build/client 127.0.0.1 $PORT > $CLIENT1_LOG 2>&1 &
CLIENT1_PID=$!
sleep 1

echo "启动客户端2（长连接）..."
(sleep 100) | ./build/client 127.0.0.1 $PORT > $CLIENT2_LOG 2>&1 &
CLIENT2_PID=$!
sleep 1

# 获取客户端的fd
echo "list" > $SERVER_FIFO
sleep 0.5

# 从服务器日志中提取fd
CLIENT1_FD=$(grep -m1 "客户端已连接" $SERVER_LOG | grep -o '\[fd:[0-9]\+\]' | head -1 | grep -o '[0-9]\+')
CLIENT2_FD=$(grep "客户端已连接" $SERVER_LOG | grep -o '\[fd:[0-9]\+\]' | tail -1 | grep -o '[0-9]\+')

echo "客户端1 fd: $CLIENT1_FD"
echo "客户端2 fd: $CLIENT2_FD"

# 向客户端1发送消息
echo "send $CLIENT1_FD Hello Client 1" > $SERVER_FIFO
sleep 0.5

# 向客户端2发送消息
echo "send $CLIENT2_FD Hello Client 2" > $SERVER_FIFO
sleep 0.5

# 广播消息
echo "broadcast Hello everyone" > $SERVER_FIFO
sleep 0.5

# 关闭客户端和服务器
kill $CLIENT1_PID $CLIENT2_PID 2>/dev/null
sleep 0.5
kill -SIGINT $SERVER_PID 2>/dev/null
sleep 0.5

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

echo "=== 验证 ==="

# 验证list命令输出
if grep -q '\[fd:[0-9]\+\] (地址=' $SERVER_LOG; then
    echo "✓ list命令显示fd格式"
else
    echo "✗ list命令格式错误"
fi

# 验证send命令
if grep -q "已向 \[fd:$CLIENT1_FD\] 发送消息" $SERVER_LOG; then
    echo "✓ send命令使用fd标识"
else
    echo "✗ send命令格式错误"
fi

# 验证客户端收到的消息
if grep -q "Hello Client 1" $CLIENT1_LOG; then
    echo "✓ 客户端1收到定向消息"
else
    echo "✗ 客户端1未收到消息"
fi

if grep -q "Hello Client 2" $CLIENT2_LOG; then
    echo "✓ 客户端2收到定向消息"
else
    echo "✗ 客户端2未收到消息"
fi

if grep -q "Hello everyone" $CLIENT1_LOG && grep -q "Hello everyone" $CLIENT2_LOG; then
    echo "✓ 广播消息正常工作"
else
    echo "✗ 广播消息失败"
fi

echo ""
echo "测试完成！"
