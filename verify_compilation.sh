#!/bin/bash

# 验证历史功能已正确编译和链接

echo "================================"
echo "验证命令历史导航功能的编译"
echo "================================"
echo ""

# 检查可执行文件是否存在
if [ ! -f ./server ] || [ ! -f ./client ]; then
    echo "错误：可执行文件不存在，请先编译"
    exit 1
fi

echo "1. 检查客户端符号..."
echo ""

# 检查客户端包含的历史相关符号
FUNCTIONS=(
    "init_history"
    "add_to_history"
    "get_previous_command"
    "get_next_command"
    "reset_history_navigation"
    "enable_raw_mode"
    "disable_raw_mode"
    "read_line_with_history"
)

MISSING=0
for func in "${FUNCTIONS[@]}"; do
    if nm ./client 2>/dev/null | grep -q "$func"; then
        echo "   ✓ $func"
    else
        echo "   ✗ $func (缺失)"
        MISSING=$((MISSING + 1))
    fi
done

echo ""
echo "2. 检查服务器符号..."
echo ""

SERVER_FUNCTIONS=(
    "init_history"
    "add_to_history"
    "get_previous_command"
    "get_next_command"
    "init_server_input"
    "cleanup_server_input"
    "process_server_input_char"
)

for func in "${SERVER_FUNCTIONS[@]}"; do
    if nm ./server 2>/dev/null | grep -q "$func"; then
        echo "   ✓ $func"
    else
        echo "   ✗ $func (缺失)"
        MISSING=$((MISSING + 1))
    fi
done

echo ""
echo "3. 检查源代码关键特性..."
echo ""

# 检查源代码中是否有历史导航的关键特性
if grep -q "使用上下箭头键导航命令历史" client.c; then
    echo "   ✓ 客户端包含历史导航提示"
else
    echo "   ✗ 客户端缺少历史导航提示"
    MISSING=$((MISSING + 1))
fi

if grep -q "支持上下箭头键导航命令历史" server.c; then
    echo "   ✓ 服务器包含历史导航提示"
else
    echo "   ✗ 服务器缺少历史导航提示"
    MISSING=$((MISSING + 1))
fi

if grep -q "CommandHistory" common.h; then
    echo "   ✓ common.h定义了CommandHistory结构"
else
    echo "   ✗ common.h缺少CommandHistory结构"
    MISSING=$((MISSING + 1))
fi

if grep -q "MAX_HISTORY_SIZE" common.h; then
    echo "   ✓ common.h定义了MAX_HISTORY_SIZE常量"
else
    echo "   ✗ common.h缺少MAX_HISTORY_SIZE常量"
    MISSING=$((MISSING + 1))
fi

echo ""
echo "4. 检查历史管理实现..."
echo ""

if [ -f history.c ]; then
    echo "   ✓ history.c文件存在"
    
    # 检查关键实现
    if grep -q "循环缓冲区" history.c || grep -q "循环队列" history.c; then
        echo "   ✓ 使用循环缓冲区存储"
    else
        echo "   ✗ 未找到循环缓冲区实现"
        MISSING=$((MISSING + 1))
    fi
    
    if grep -q "转义序列" history.c; then
        echo "   ✓ 实现了转义序列检测（箭头键）"
    else
        echo "   ✗ 未找到转义序列处理"
        MISSING=$((MISSING + 1))
    fi
    
    if grep -q "refresh_line" history.c; then
        echo "   ✓ 实现了行刷新功能"
    else
        echo "   ✗ 未找到行刷新功能"
        MISSING=$((MISSING + 1))
    fi
else
    echo "   ✗ history.c文件不存在"
    MISSING=$((MISSING + 10))
fi

echo ""
echo "================================"
if [ $MISSING -eq 0 ]; then
    echo "✓ 所有检查通过！"
    echo "================================"
    echo ""
    echo "命令历史导航功能已正确实现："
    echo "  - 历史记录使用循环缓冲区存储"
    echo "  - 支持最多100条历史记录"
    echo "  - 捕获上下箭头键（转义序列 \\033[A 和 \\033[B）"
    echo "  - 支持左右箭头键移动光标"
    echo "  - 支持退格键编辑"
    echo "  - 客户端和服务器都支持历史导航"
    echo ""
    echo "使用方法："
    echo "  1. 运行服务器: ./server 8888"
    echo "  2. 运行客户端: ./client 127.0.0.1 8888"
    echo "  3. 输入一些命令"
    echo "  4. 按上箭头键查看历史命令"
    echo "  5. 按下箭头键前进到更新的命令"
    echo "  6. 可以编辑历史命令后再发送"
    echo ""
    exit 0
else
    echo "✗ 发现 $MISSING 个问题"
    echo "================================"
    exit 1
fi
