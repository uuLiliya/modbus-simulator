#!/bin/bash

# 测试命令历史导航功能

set -e

PORT=9999

# 清理函数
cleanup() {
    echo "清理测试环境..."
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
    wait 2>/dev/null || true
}

trap cleanup EXIT

echo "================================"
echo "测试命令历史导航功能"
echo "================================"

# 启动服务器
echo ""
echo "1. 启动服务器（端口 $PORT）..."
./build/server $PORT > server_test.log 2>&1 &
SERVER_PID=$!
sleep 1

# 检查服务器是否成功启动
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "错误：服务器启动失败"
    cat server_test.log
    exit 1
fi
echo "   服务器已启动 (PID: $SERVER_PID)"

# 测试客户端和历史功能
echo ""
echo "2. 测试客户端连接..."
echo ""
echo "================================"
echo "手动测试步骤："
echo "================================"
echo ""
echo "请打开新的终端窗口，运行："
echo "   ./build/client 127.0.0.1 $PORT"
echo ""
echo "然后测试以下功能："
echo ""
echo "  1) 输入一些命令："
echo "     - hello"
echo "     - world"
echo "     - modbus read 0 5"
echo "     - test message"
echo ""
echo "  2) 按【上箭头】键，应该看到："
echo "     - 首先显示 'test message'"
echo "     - 再按一次显示 'modbus read 0 5'"
echo "     - 继续按显示 'world'"
echo "     - 继续按显示 'hello'"
echo ""
echo "  3) 按【下箭头】键，应该能够："
echo "     - 向前浏览到更新的命令"
echo "     - 最终回到空输入状态"
echo ""
echo "  4) 可以选择历史命令后修改再发送"
echo ""
echo "  5) 输入 quit 退出"
echo ""
echo "同样的功能也可以在服务器端测试："
echo "  - 在服务器终端输入命令如 'list', 'help'"
echo "  - 使用上下箭头键浏览历史"
echo ""
echo "================================"
echo ""
echo "按 Ctrl+C 停止服务器"

# 等待用户手动测试
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "测试结束"
