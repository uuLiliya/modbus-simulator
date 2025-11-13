#!/bin/bash

# 测试多客户端同时连接，验证服务器能区分不同的fd

PORT=15556
SERVER_LOG=test_multi_server.log

echo "启动服务器..."
./build/server $PORT > $SERVER_LOG 2>&1 &
SERVER_PID=$!
sleep 1

echo "同时连接3个客户端..."

# 客户端1
(sleep 1; echo "Message from C1"; sleep 2; echo "quit") | ./build/client 127.0.0.1 $PORT > /dev/null 2>&1 &
C1_PID=$!

# 客户端2
(sleep 1.2; echo "Message from C2"; sleep 2; echo "quit") | ./build/client 127.0.0.1 $PORT > /dev/null 2>&1 &
C2_PID=$!

# 客户端3
(sleep 1.4; echo "Message from C3"; sleep 2; echo "quit") | ./build/client 127.0.0.1 $PORT > /dev/null 2>&1 &
C3_PID=$!

# 等待所有客户端完成
wait $C1_PID 2>/dev/null
wait $C2_PID 2>/dev/null
wait $C3_PID 2>/dev/null

# 关闭服务器
kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "=== 服务器日志 ==="
cat $SERVER_LOG
echo ""

# 验证
echo "=== 验证 ==="

# 提取所有连接的fd
FDS=$(grep "客户端已连接" $SERVER_LOG | grep -o '\[fd:[0-9]\+\]' | grep -o '[0-9]\+' | sort -u)
FD_COUNT=$(echo "$FDS" | wc -l)

echo "检测到 $FD_COUNT 个不同的文件描述符："
echo "$FDS"
echo ""

# 验证每个客户端的消息是否使用对应的fd
echo "验证消息标识："
for fd in $FDS; do
    if grep -q "\[fd:$fd\] 消息：" $SERVER_LOG; then
        echo "✓ fd:$fd 的消息被正确标识"
    else
        echo "✗ fd:$fd 的消息未找到"
    fi
done
echo ""

# 验证断开连接的fd
echo "验证断开连接："
for fd in $FDS; do
    if grep -q "\[fd:$fd\] 已断开连接" $SERVER_LOG; then
        echo "✓ fd:$fd 的断开连接被正确记录"
    else
        echo "✗ fd:$fd 的断开连接未找到"
    fi
done

# 清理
rm -f $SERVER_LOG

echo ""
echo "测试完成！"
