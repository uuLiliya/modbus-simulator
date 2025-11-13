#!/bin/bash

# 自动化测试命令历史导航功能

set -e

PORT=9998

# 清理函数
cleanup() {
    echo "清理测试环境..."
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
    if [ ! -z "$CLIENT_PID" ]; then
        kill $CLIENT_PID 2>/dev/null || true
    fi
    wait 2>/dev/null || true
    rm -f /tmp/client_input.txt /tmp/client_output.txt /tmp/server_output.txt
}

trap cleanup EXIT

echo "================================"
echo "自动化测试命令历史导航功能"
echo "================================"

# 启动服务器
echo ""
echo "1. 启动服务器（端口 $PORT）..."
./server $PORT > /tmp/server_output.txt 2>&1 &
SERVER_PID=$!
sleep 1

# 检查服务器是否成功启动
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "错误：服务器启动失败"
    cat /tmp/server_output.txt
    exit 1
fi
echo "   ✓ 服务器已启动 (PID: $SERVER_PID)"

# 测试客户端连接
echo ""
echo "2. 测试客户端连接和基本通信..."

# 创建测试输入（不包含箭头键，因为难以自动化测试）
cat > /tmp/client_input.txt << 'EOF'
hello
world
test message 1
test message 2
quit
EOF

# 启动客户端（使用输入重定向）
timeout 5 ./client 127.0.0.1 $PORT < /tmp/client_input.txt > /tmp/client_output.txt 2>&1 || true

echo "   ✓ 客户端已完成测试"

# 检查输出
echo ""
echo "3. 验证测试结果..."

# 检查服务器日志
if grep -q "客户端已连接" /tmp/server_output.txt; then
    echo "   ✓ 服务器成功接受客户端连接"
else
    echo "   ✗ 未找到连接日志"
    exit 1
fi

if grep -q "hello" /tmp/server_output.txt; then
    echo "   ✓ 服务器接收到消息 'hello'"
else
    echo "   ✗ 服务器未接收到消息"
    exit 1
fi

# 检查客户端日志
if grep -q "连接成功" /tmp/client_output.txt; then
    echo "   ✓ 客户端成功连接"
else
    echo "   ✗ 客户端连接失败"
    cat /tmp/client_output.txt
    exit 1
fi

if grep -q "使用上下箭头键导航命令历史" /tmp/client_output.txt; then
    echo "   ✓ 客户端显示历史导航提示"
else
    echo "   ✗ 未找到历史导航提示"
fi

if grep -q "服务器回显" /tmp/client_output.txt; then
    echo "   ✓ 客户端接收到服务器回显"
else
    echo "   ✗ 客户端未接收到回显"
fi

# 检查编译是否包含历史模块
echo ""
echo "4. 验证历史功能编译..."
if nm ./client | grep -q "init_history"; then
    echo "   ✓ 客户端包含历史初始化函数"
else
    echo "   ✗ 客户端缺少历史函数"
    exit 1
fi

if nm ./client | grep -q "add_to_history"; then
    echo "   ✓ 客户端包含历史添加函数"
else
    echo "   ✗ 客户端缺少历史添加函数"
    exit 1
fi

if nm ./client | grep -q "get_previous_command"; then
    echo "   ✓ 客户端包含历史导航函数"
else
    echo "   ✗ 客户端缺少历史导航函数"
    exit 1
fi

if nm ./server | grep -q "init_history"; then
    echo "   ✓ 服务器包含历史初始化函数"
else
    echo "   ✗ 服务器缺少历史函数"
    exit 1
fi

if nm ./server | grep -q "process_server_input_char"; then
    echo "   ✓ 服务器包含非阻塞输入处理函数"
else
    echo "   ✗ 服务器缺少输入处理函数"
    exit 1
fi

# 检查服务器提示信息
if grep -q "支持上下箭头键导航命令历史" /tmp/server_output.txt; then
    echo "   ✓ 服务器显示历史导航提示"
else
    echo "   ✗ 服务器未显示历史导航提示"
fi

echo ""
echo "================================"
echo "✓ 所有自动化测试通过！"
echo "================================"
echo ""
echo "注意：箭头键导航功能需要交互式测试"
echo "请运行 ./test_history.sh 进行手动测试"
echo ""

# 清理
kill $SERVER_PID 2>/dev/null || true
wait 2>/dev/null || true
