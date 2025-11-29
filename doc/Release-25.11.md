# Easy Graphics Engine 25.11 发布公告

**发布日期**: 2025 年 11 月

我们非常高兴地宣布 **EGE (Easy Graphics Engine) 25.11** 正式发布！这是继 24.04 版本之后的又一重要更新，带来了大量令人兴奋的新特性、性能改进和问题修复。

---

## 🎯 版本亮点

### 📸 相机捕捉功能（全新功能）

本次更新最大的亮点是引入了**摄像头/相机捕捉功能**！新增 `ege::Camera` 类，基于 [ccap](https://github.com/wysaid/CameraCapture) 提供相机驱动，支持在 C++17 及以上的编译器中使用，让您可以轻松在 EGE 程序中获取摄像头画面。

### 📦 图像库升级

- 使用 **stb_image** 替代 `libpng` 解析图像文件，支持更多图像格式（PNG、JPEG、BMP、GIF、TGA、PSD、HDR、PIC、PNM）
- 使用轻量的 **sdefl/sinfl** 库替代 `zlib` 实现数据压缩/解压，移除 `libpng` 和 `zlib` 依赖，简化编译配置

---

## ✨ 新增功能

### 鼠标输入增强

- 新增鼠标**双击检测**支持，`mouse_msg::is_doubleclick()` 方法
- 新增鼠标**扩展键检测**（如 XBUTTON1, XBUTTON2）

### 键盘输入增强

- 新增快速检测按键动作的函数：`keypress`、`keyrelease`、`keyrepeat`
- 新增 `kbhit_console` 函数，与 `getch_console` 配套使用

### 文本与图像处理

- 新增 `ege::measuretext` 函数，用于测量 `ege_` 前缀函数绘制的文本宽高
- 新增 `ege::image_convertcolor` 函数，用于转换图像像素的颜色类型
- 新增 `color_type` 枚举，用于指定图像像素的颜色类型（`COLORTYPE_PRGB32`, `COLORTYPE_ARGB32`, `COLORTYPE_RGB32`）

### 数据压缩

- 新增 `ege_compress_bound` 函数，用于获取压缩数据的最大长度

### 其他新功能

- 新增 `ege/types.h` 头文件，提供基础类型定义

---

## 🔧 改进优化

### MSVC Debug/Release 库分离

由于相机捕捉功能（ccap）使用了 C++17 特性和大量 STL，MSVC 的 Debug 和 Release 配置存在运行时库不兼容问题（`_ITERATOR_DEBUG_LEVEL` 值不同、容器内存布局不同等）。本版本为 MSVC 提供了独立的 Debug 版本静态库：

| 编译器 | Release 配置 | Debug 配置 |
|--------|-------------|-----------|
| MSVC | `graphics.lib` | `graphicsd.lib` |
| MinGW | `libgraphics.a` | （无需区分） |

头文件通过 `_DEBUG` 宏自动选择正确的库，**用户无需手动配置**。

### 渲染性能提升

- 图像像素默认颜色格式由 `ARGB32` 改为 `PRGB32`（预乘Alpha），**显著提升渲染效率**
- 优化静态库编译参数，同时支持 `/MD` 和 `/MT` 编译

### API 改进

- `color_t` 类型调整为 `uint32_t`，提高代码一致性
- C++ 标准不低于 C++11 时，颜色枚举 `COLORS` 底层类型设置为 `uint32_t`
- `ege_` 系列文本输出函数的坐标参数类型由 `int` 改为 `float`，支持更精细的定位
- 图像读取和保存返回更详细的错误信息，新增 `grInvalidFileFormat` 和 `grUnsupportedFormat` 错误码

### 兼容性提升

- 增加对 **Visual Studio 2026** 的支持
- 优化项目配置，在编译器支持 C++17 时自动开启 C++17，并定义宏 `EGE_ENABLE_CPP17=1`
- `ege.h` 提供双语版本（英文版为默认版本，中文版 `ege.zh_CN.h` 主要用于生成文档）
- 文档示例代码中的 `sprintf` 改为更安全的 `snprintf`

---

## 🐛 问题修复

- 修复垂直方向对齐对 `ege_` 文本绘制函数无效的问题
- 修复调用 `resizewindow` 后图形出现偏移和被裁剪的问题
- 修复 `resizewindow` 后视口未进行调整的问题
- 修复 `ege_fillroundrect()` 单圆角半径参数重载无法在指定图像中绘制的问题
- 修复使用 `lineto` 前未调整当前点的问题
- 修复 `bar3d` 图形在线条连接点处有突出，以及在图形堆叠时存在重复边的问题
- 修复自动渲染模式下长时间无绘制操作时不触发刷新的问题
- 修复 `IMAGE::gentexture` 会引发栈溢出的问题
- 修复调用 `delay_ms` 时出现帧率误差较大的问题
- 修复 `getpixel_f` 和 `putpixels_f` 声明和定义不一致的问题
- 修复 `MUSIC` 需要依赖 `initgraph()` 初始化的问题
- 修复部分函数使用 `ege_transform_matrix` 参数做变换时崩溃的问题
- 修复 `ege/button.h` 中调用 `setfillstyle` 时参数位置错误的问题
- 纠正 `PIMAGE` 和 `PCIMAGE` 的错误混用

---

## 🔄 调整变更

### 类型与枚举调整

- `alpha_type` 枚举值顺序调整：`ALPHATYPE_PREMULTIPLIED` 现为 0，`ALPHATYPE_STRAIGHT` 为 1
- `putimage_alphablend` 系列函数的参数由 `alpha_type alphaType` 改为 `color_type colorType`，默认值为 `COLORTYPE_PRGB32`
- 数据压缩相关函数签名调整，参数类型由 `unsigned long` 改为 `uint32_t`
- `Bound`、`Rect`、`RectF` 类的方法重命名：`isContains` → `contains`，`isOverlaps` → `overlaps`
- 修正拼写错误：`tranpose` → `transpose`，`flipHorizonal` → `flipHorizontal`

### 行为调整

- `keystate` 返回值改为 `bool` 类型，参数无效时返回 `false`
- `kbmsg`、`kbhit` 在运行环境退出后的返回值由 `-1` 改为 `0`，防止阻塞
- 非阻塞或非延时函数不再触发窗口刷新
- `flushmouse`、`flushkey` 不再触发窗口刷新
- 调整初始化环境之前所返回的颜色值，允许预先设置窗口背景色
- `graphics_errors` 数值改为十进制格式，便于调试

---

## 🎮 新增示例程序

本版本新增以下示例程序：

| Demo | 说明 |
|------|------|
| [camera_base](https://github.com/x-ege/xege/blob/master/demo/camera_base.cpp) | 📷 **相机基础演示** - 摄像头画面采集与显示 |
| [camera_wave](https://github.com/x-ege/xege/blob/master/demo/camera_wave.cpp) | 🌊 **相机波浪效果** - 演示相机捕捉与实时图像处理 |
| [game_gomoku](https://github.com/x-ege/xege/blob/master/demo/game_gomoku.cpp) | 🎯 **五子棋游戏** - 支持简单 AI 对战、落棋音效、抗锯齿棋子 |
| [graph_sort_visualization](https://github.com/x-ege/xege/blob/master/demo/graph_sort_visualization.cpp) | 📊 **排序可视化** - 直观展示各种排序算法的执行过程 |
| [graph_function_visualization](https://github.com/x-ege/xege/blob/master/demo/graph_function_visualization.cpp) | 📈 **函数绘制** - 蒙特卡洛法绘制二元函数图像 |
| [graph_astar_pathfinding](https://github.com/x-ege/xege/blob/master/demo/graph_astar_pathfinding.cpp) | 🗺️ **A* 寻路算法** - 经典启发式搜索算法可视化，支持迷宫生成 |
| [graph_boids](https://github.com/x-ege/xege/blob/master/demo/graph_boids.cpp) | 🐦 **Boids 群集模拟** - 模拟鸟群/鱼群的涌现行为 |
| [graph_game_of_life](https://github.com/x-ege/xege/blob/master/demo/graph_game_of_life.cpp) | 🧬 **康威生命游戏** - 元胞自动机经典演示，内置多种预设图案 |
| [graph_kmeans](https://github.com/x-ege/xege/blob/master/demo/graph_kmeans.cpp) | 🔵 **K-Means 聚类** - 机器学习聚类算法可视化 |

此外，示例库还包含更多精彩 Demo（分形绘制、时钟、星空效果等），欢迎探索！

---

## 🔨 构建与文档

- 移除 `libpng` 和 `zlib` 依赖，简化编译配置
- 优化 CMake 配置，添加构建选项允许设置不构建示例程序
- 优化 GitHub Actions 工作流，增加 MinGW Windows 构建和 macOS 交叉编译支持
- 增加编译测试模块，用于发现编译兼容性问题
- 添加单元测试相关模块和性能测试逻辑
- 优化发布脚本，支持跨平台（macOS/Linux/Windows）
- 新增 `test-run-demos.sh` 脚本，支持自动化运行示例程序进行冒烟测试，带超时控制
- 文档更新：修正多处文档错误，更新示例代码，补充新功能说明

---

## 🔌 IDE 插件

为了让开发者更便捷地使用 EGE，我们提供了主流 IDE 的官方插件支持：

### CLion 插件（全新发布）

**XEGE Creator** 是专为 JetBrains CLion 打造的 EGE 项目向导插件，帮助你快速创建和配置 EGE 图形程序项目。

| 信息 | 链接 |
|------|------|
| 插件商店 | [JetBrains Marketplace](https://plugins.jetbrains.com/plugin/28785-xege-creator) |
| 源码仓库 | <https://github.com/x-ege/ege-clion-plugin> |

**功能特性**：

- ✨ 一键创建 EGE 项目，自动配置 CMake 构建系统
- 🔧 支持预编译库模式和源码模式两种使用方式
- 🌍 完整的多平台支持（Windows MinGW/MSVC、macOS、Linux）
- 🌐 中英文双语界面，根据系统语言自动切换

**安装方式**：

1. 打开 CLion → `Settings/Preferences → Plugins`
2. 在 `Marketplace` 搜索 "XEGE Creator"
3. 点击 `Install` 安装并重启 CLion

### VSCode 插件

**EGE** 插件为 Visual Studio Code 用户提供一站式开发体验。

| 信息 | 链接 |
|------|------|
| 插件商店 | [VS Marketplace](https://marketplace.visualstudio.com/items?itemName=wysaid.ege) |
| 源码仓库 | <https://github.com/x-ege/ege-vscode-plugin> |

**功能特性**：

- 📁 一键生成 EGE 项目（支持预编译库和源码两种模板）
- ▶️ 单文件快速编译运行，无需复杂配置
- 🍎 支持 macOS/Linux（通过 mingw-w64 + wine 跨平台编译）
- 🌐 中英文双语界面，自动适配 VS Code 语言设置

**安装方式**：

1. 打开 VS Code → 扩展面板（`Ctrl+Shift+X`）
2. 搜索 "ege" 或 "xege"
3. 点击安装

---

## 📦 下载与安装

### 预编译 SDK

查看: <https://xege.org/install_and_config>

### 仓库地址

- **GitHub**: <https://github.com/x-ege/xege>
- **SDK 仓库**: <https://github.com/x-ege/xege-sdk>
- **官网**: <https://xege.org>

### 环境要求

- Windows 7 及以上系统
- 支持的编译器：Visual Studio 2010-2026, GCC/MinGW

---

## 🙏 致谢

感谢所有贡献者和用户的支持！EGE 致力于为 C/C++ 初学者提供最简单易用的图形编程体验。

如有问题或建议，欢迎访问 [GitHub Issues](https://github.com/x-ege/xege/issues) 反馈。

---

Happy Coding with EGE! 🎉
