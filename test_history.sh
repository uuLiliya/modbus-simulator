#!/bin/bash

echo "=== 命令历史功能测试脚本 ==="
echo ""

# 清理之前的进程
pkill -f "build/server" 2>/dev/null
pkill -f "build/client" 2>/dev/null

# 编译项目
echo "1. 编译项目..."
make clean && make
if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi
echo "✅ 编译成功"
echo ""

# 启动服务器
echo "2. 启动服务器..."
./build/server 8888 &
SERVER_PID=$!
sleep 2

# 检查服务器是否运行
if ! ps -p $SERVER_PID > /dev/null; then
    echo "❌ 服务器启动失败"
    exit 1
fi
echo "✅ 服务器启动成功 (PID: $SERVER_PID)"
echo ""

# 测试客户端
echo "3. 测试客户端历史功能..."
echo "   请手动测试以下功能："
echo "   - 输入几个不同的命令"
echo "   - 按上箭头键查看历史"
echo "   - 按下箭头键返回"
echo "   - 使用backspace编辑历史命令"
echo "   - 按Ctrl+C优雅退出"
echo ""

./build/client 127.0.0.1 8888

# 清理
echo ""
echo "4. 清理进程..."
kill $SERVER_PID 2>/dev/null
pkill -f "build/server" 2>/dev/null
pkill -f "build/client" 2>/dev/null

echo ""
echo "=== 测试完成 ==="
echo "如果所有功能都正常工作，说明命令历史功能已成功修复！"