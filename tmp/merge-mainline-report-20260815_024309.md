# 主干合并冲突解决报告

## 1. 基本信息

- 当前分支：`codex/coregraphics-native-backend`
- 当前分支 upstream：`origin/codex/coregraphics-native-backend`
- 远端名：`origin`
- 探测到的主干分支：`origin/master`
- 合并命令：`GIT_MERGE_AUTOEDIT=no git merge --no-edit origin/master`
- 报告时间：2026-08-15 02:43:09 CST

## 2. 冲突文件

- `src/egegapi.cpp`
- `src/font.cpp`
- `src/graphics.cpp`
- `src/image.cpp`
- `src/keyboard.cpp`
- `src/music.cpp`

此外，`cmake/EgeSources.cmake`、`src/image_ex.cpp` 和
`include/ege/win32_compat.h` 存在未被 Git 标记的语义冲突，并在验证阶段同步修正。

## 3. 当前分支独有 commit

- `eb6be5c fix: complete CoreGraphics native backend coverage`
- `d46c2f9 Merge remote-tracking branch 'origin/master' into codex/coregraphics-native-backend`
- `6b54e3e fix: harden native macOS build and headless tests`
- `e0870b1 feat: add native macOS CoreGraphics backend`

## 4. 主干独有 commit

- `17be0a2 fix: 减少vs2026构建警告`
- `a06316a fix: 修复程序自动结束时进程残留`
- `896d559 清理少量无用代码`
- `f18b8cf ege_path模块分离，编译体积预计降10K`
- `9f97f1d fix: 修复resizewindow()扩大窗口时闪烁白边`
- `72888a7 stb_image模块分离，编译体积预计降350K`
- `eb79597 移除无用的空文件 (#379)`
- `2eb2bc2 Merge pull request #374 from FeJS8888/fix-syskey-error`
- `574c108 Merge pull request #371 from pointertobios/bugfix/process-cannot-exit`
- `c7e317a fix dead lock`
- `7723583 process cannot exit`
- `dda1352 添加注释记录拦截 WM_SYSKEYDOWN(UP) 事件`
- `cf8b66c 添加注释记录 WM_KEYUP 分支不可达现象`
- `8662536 fix: 修复未处理 WM_SYSKEYDOWN/WM_SYSKEYUP 导致的异常`

## 5. 冲突原因分析

### `src/egegapi.cpp`

- 当前分支改了什么：在旧文件布局上完善 CoreGraphics/GDI+ 兼容 API。
- 主干改了什么：把完整 `ege_path` 实现拆到 `src/ege_path.cpp`。
- 为什么会冲突：主干删除了当前分支仍在修改的大段路径实现。

### `src/font.cpp` 与 `src/music.cpp`

- 当前分支改了什么：增加 macOS 文本、音乐后端依赖和标准库依赖。
- 主干改了什么：删除空的私有头 `font.h`、`music.h`。
- 为什么会冲突：双方同时修改了文件头部 include 区域。

### `src/graphics.cpp`

- 当前分支改了什么：加入原生窗口生命周期、无窗口刷新和 UI 线程安全退出逻辑。
- 主干改了什么：修复进程退出残留，并把窗口 resize 调整到交换缓冲区之前。
- 为什么会冲突：双方修改了析构、`graphupdate` 和消息线程入口的相同代码段。

### `src/image.cpp`

- 当前分支改了什么：补齐非 Windows 文件打开、内存解码、CPU 像素缓冲和透明度兼容。
- 主干改了什么：把 stb_image 文件编解码实现拆到 `src/image_ex.cpp`。
- 为什么会冲突：主干移动的函数正是当前分支跨平台修改覆盖的区域。

### `src/keyboard.cpp`

- 当前分支改了什么：区分原生文本回调与控制键，避免丢失 Escape、Enter、Backspace。
- 主干改了什么：加入 `WM_SYSKEYDOWN` / `WM_SYSKEYUP` 支持。
- 为什么会冲突：双方修改了 `peekkey` 的同一判断条件。

## 6. 解决方案

### 模块拆分

- 最终保留了哪些行为：保留主干的 `ege_path.cpp`、`image_ex.cpp` 拆分。
- 舍弃了哪些内容，以及为什么可以舍弃：删除旧文件中的重复实现，因为对应代码已迁入新模块。
- 同步调整：更新显式 CMake 源清单；`ege_path.cpp` 仅在 Windows 构建，非 Windows 继续使用 `ege_gdiplus_fallback.cpp`。

### 图像兼容

- 最终保留了哪些行为：UTF-8 宽路径打开、非 GDI+ 解码、可写缓冲区访问、零 alpha 的 RGB32 输出兼容。
- 舍弃了哪些内容，以及为什么可以舍弃：没有舍弃已确认的跨平台行为，只把实现迁移到主干新模块。
- 同步调整：将非 Windows 内存解码封装到 `image_ex.cpp`，Windows GDI+ bitmap 路径保持条件编译。

### 窗口与键盘

- 最终保留了哪些行为：Windows UI 线程安全退出、主干 resize-before-swap 修复、macOS swap 路径、系统键消息和原生控制键行为。
- 舍弃了哪些内容，以及为什么可以舍弃：舍弃会在 macOS 上调用 Win32 API 的无条件代码路径。
- 同步调整：在 `win32_compat.h` 增加标准 Win32 系统键消息常量。

## 7. 验证结果

- `git diff --cached --check`：通过。
- CoreGraphics Release 配置：通过。
- `cmake --build build/mac-tests --parallel 4`：通过。
- `cmake --build build/mac-tests --target demos --parallel 4`：通过。
- `ctest --test-dir build/mac-tests --output-on-failure`：13/13 通过。
- 构建中仅出现现有第三方库、SDK 和 demo 的弃用/代码风格警告，没有错误。

## 8. 最终状态

- 主干 merge commit：`2e49598520a2514e5dd278f898a27e3b0e59386d`
- Push 结果：已成功推送到 `origin/codex/coregraphics-native-backend`（`eb6be5c..2e49598`）。
- 报告交付：本报告使用单独的 report-only commit 提交并推送。
- 是否还需要用户跟进：否。
