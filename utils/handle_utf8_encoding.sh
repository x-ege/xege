#!/usr/bin/env bash
# 为老版本 MSVC 编译器添加 UTF-8 BOM 的脚本

# 检查是否为 MSVC 2010-2013
check_old_msvc() {
    local toolset="$1"
    case "$toolset" in
    v100 | v110 | v120) # MSVC 2010, 2012, 2013
        return 0
        ;;
    *)
        return 1
        ;;
    esac
}

# 为文件添加 UTF-8 BOM
add_utf8_bom() {
    local file="$1"
    if [[ -f "$file" ]]; then
        # 检查文件是否已经有 BOM
        if ! head -c 3 "$file" | od -A n -t x1 | grep -q "ef bb bf"; then
            # 创建临时文件，添加 BOM
            printf '\xEF\xBB\xBF' >"$file.tmp"
            cat "$file" >>"$file.tmp"
            mv "$file.tmp" "$file"
            echo "Added UTF-8 BOM to: $file"
        fi
    fi
}

# 移除文件的 UTF-8 BOM
remove_utf8_bom() {
    local file="$1"
    if [[ -f "$file" ]]; then
        # 检查文件是否有 BOM
        if head -c 3 "$file" | od -A n -t x1 | grep -q "ef bb bf"; then
            # 移除 BOM
            tail -c +4 "$file" >"$file.tmp"
            mv "$file.tmp" "$file"
            echo "Removed UTF-8 BOM from: $file"
        fi
    fi
}

# 处理包含中文的源文件
process_chinese_files() {
    local action="$1" # add_bom 或 remove_bom

    # 查找包含中文字符的文件
    local chinese_files=(
        "src/utils.h"
        "src/time.cpp"
        "src/egegapi.cpp"
        "src/ege_head.h"
        "src/ege_math.h"
        "src/ege_dllimport.h"
        "src/ege_dllimport.cpp"
        "src/color.h"
        "src/ege_graph.h"
        "src/encodeconv.cpp"
        "src/font.cpp"
        "src/gdi_conv.cpp"
        "src/graphics.cpp"
        "src/image.cpp"
        "src/mouse.cpp"
        "src/random.cpp"
        # 可以在这里添加其他包含中文的文件
    )

    for file in "${chinese_files[@]}"; do
        if [[ "$action" == "add_bom" ]]; then
            add_utf8_bom "$file"
        elif [[ "$action" == "remove_bom" ]]; then
            remove_utf8_bom "$file"
        fi
    done
}

# 主函数
main() {
    local toolset="$1"
    local action="$2"

    if [[ -z "$toolset" ]]; then
        echo "Usage: $0 <toolset> [add_bom|remove_bom]"
        echo "Example: $0 v100 add_bom"
        exit 1
    fi

    if check_old_msvc "$toolset"; then
        echo "Detected old MSVC toolset: $toolset"
        if [[ "$action" == "remove_bom" ]]; then
            process_chinese_files "remove_bom"
        else
            process_chinese_files "add_bom"
        fi
    else
        echo "Modern MSVC toolset: $toolset, ensuring no BOM"
        process_chinese_files "remove_bom"
    fi
}

# 如果脚本被直接执行
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
