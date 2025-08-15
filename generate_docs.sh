#!/bin/bash
# EGE Documentation Generation Script
# 用于重新生成 EGE 图形库的文档

echo "正在生成 EGE 图形库文档..."
echo "Generating EGE Graphics Library Documentation..."

cd "$(dirname "$0")/doc"

# 清理旧文档
echo "清理旧文档文件..."
rm -rf html/ latex/

# 生成新文档
echo "运行 Doxygen..."
if command -v doxygen &> /dev/null; then
    doxygen Doxyfile
    if [ $? -eq 0 ]; then
        echo "文档生成成功！"
        echo "Documentation generated successfully!"
        echo "请打开 doc/html/index.html 或项目根目录的 index.html 查看文档"
        echo "Open doc/html/index.html or index.html in the project root to view the documentation"
    else
        echo "文档生成失败！"
        echo "Documentation generation failed!"
        exit 1
    fi
else
    echo "错误：未找到 doxygen 命令，请先安装 Doxygen"
    echo "Error: doxygen command not found. Please install Doxygen first."
    exit 1
fi