# EGE (Easy Graphics Engine)

[![MSVC Build](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml)
[![MinGW Windows Build](https://github.com/x-ege/xege/actions/workflows/mingw-windows-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/mingw-windows-build.yml)
[![MinGW Cross-Compile Build](https://github.com/x-ege/xege/actions/workflows/mingw-crosscompile-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/mingw-crosscompile-build.yml)
[![License](https://img.shields.io/badge/license-LGPL--2.1-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://github.com/x-ege/xege)

EGE (Easy Graphics Engine) 是一个 Windows 下的简易绘图库，提供类似 BGI (`graphics.h`) 的接口，专为 C/C++ 初学者设计。

## 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [支持的开发环境](#支持的开发环境)
- [文档与资源](#文档与资源)
- [为什么选择 EGE](#为什么选择-ege)
- [构建指南](#构建指南)
- [社区与支持](#社区与支持)
- [历史信息](#历史信息)

## 特性

- **简单易用** — 接口设计直观，与 TC 的 `graphics.h` 高度兼容，初学者也能快速上手
- **高效渲染** — 窗口锁定绘图模式下，640×480 的半透明混合可达 60 FPS
- **功能丰富** — 支持拉伸贴图、图片旋转、透明/半透明贴图、图像滤镜、多种图片格式（BMP/JPG/PNG/GIF 等）
- **灵活绘图** — 绘图可直接针对图像进行，不限于屏幕
- **免费开源** — 采用 LGPL 2.1 许可证，欢迎参与开发

## 快速开始

```cpp
#include <graphics.h>

int main()
{
    initgraph(640, 480);       // 初始化窗口
    circle(200, 200, 100);     // 画圆：圆心 (200, 200)，半径 100
    getch();                   // 等待按键
    closegraph();              // 关闭图形窗口
    return 0;
}
```

EGE 模拟了绝大多数 BGI 绘图函数，基本绘图函数的使用方式与 Turbo C / Borland C 基本一致。

> 更多示例请参阅 [官方教程](https://xege.org/beginner-lesson-1.html) 和 [示例程序](https://xege.org/category/demo)。

## 支持的开发环境

EGE 支持以下 开发工具/编译器：

| 编译器/IDE | 版本支持 | 备注 |
|------------|----------|------|
| [Visual Studio](https://visualstudio.microsoft.com/) | 2017 ~ 2026 | ✅ **推荐**，支持 C++17 全部特性 </br> 强烈建议使用最新版本 (2026) |
| [CLion](https://www.jetbrains.com/clion/) | 支持 | ✅ **推荐** 可配合 [XEGE 插件](#ide-插件) 迅速上手 |
| [VS Code](https://code.visualstudio.com/) | 支持 | ✅ **推荐** 可配合 [EGE 插件](#ide-插件) 迅速上手 |
| [小熊猫 C++](http://royqh.net/redpandacpp/) | 支持 | ✅ **推荐** 已内置 EGE, 下载即可用 |
| Code::Blocks | 支持 | 已测试版本: 25.03 (最新) |
| MinGW / MinGW-w64 | 支持 | ✅ 支持, SDK 发布时会基于最新版本进行测试 |
| 老版本 Visual Studio | 2010 ~ 2015 | ⚠️ 支持，但不推荐（不支持 C++17） |
| Visual C++ 6.0 | aka vc6.0 | ⚠️ 旧版支持，可从[官网](https://xege.org/install_and_config)下载内嵌版本 |
| Dev-C++ | 支持 | ⚠️ 十年未更新, 不那么推荐 </br> 已测试版本: 5.11 (最新) |
| Eclipse for C/C++、 C-Free 等 | 支持 | ⚠️ 需自行配置 |

## 文档与资源

- **API 文档**：[API 参考](man/api.md)
- **详细帮助**：`man` 目录下的 `index.htm`

## IDE 插件

EGE 提供官方 IDE 插件，让项目配置更加简单：

| IDE | 插件 | 安装方式 | 功能 |
|-----|------|---------|------|
| **CLion** | [XEGE Creator](https://plugins.jetbrains.com/plugin/28785-xege-creator) | `Settings → Plugins → Marketplace` 搜索 "XEGE Creator" | 一键创建项目、CMake 自动配置、多平台支持 |
| **VS Code** | [EGE](https://marketplace.visualstudio.com/items?itemName=wysaid.ege) | 扩展面板搜索 "ege" | 一键创建项目、单文件编译运行、跨平台支持 |

### CLion 插件使用

1. 安装插件后，点击 `File → New → Project...`
2. 在左侧项目类型列表中选择 **EGE**
3. 配置项目名称和位置，选择是否使用源码模式
4. 点击 `Create` 即可生成可运行的 EGE 项目

### VS Code 插件使用

1. 安装插件后，打开命令面板（`Ctrl+Shift+P`）
2. 输入 `EGE:` 查看可用命令：
   - `EGE: 在当前工作区设置 ege 项目` - 使用预编译库创建项目
   - `EGE: 在当前工作区设置带有 EGE 源代码的 ege 项目` - 使用源码创建项目
   - `EGE: 构建并运行当前文件` - 快速编译运行单个 cpp 文件
3. 插件支持 Windows、macOS（需 mingw-w64 + wine）和 Linux

> 更多详情请访问插件主页：[CLion 插件](https://github.com/x-ege/ege-clion-plugin) | [VS Code 插件](https://github.com/x-ege/ege-vscode-plugin)

## 为什么选择 EGE

许多学习编程的人从 C 语言开始入门，但现有的教学环境存在一些问题：

1. **Turbo C 环境过时** — DOS 环境与现代操作系统兼容性差，可用颜色有限，编辑功能不便
2. **Windows 图形编程门槛高** — 在 Windows 编程中绘制简单图形需要处理窗口类、消息循环等复杂概念
3. **图形学教学偏离重点** — 计算机图形学课程应聚焦于算法而非平台编程，但 Windows GDI 或 OpenGL 的门槛较高

**EGE 的解决方案**：无论您是 C 语言初学者、编程教师，还是计算机图形学讲师，EGE 都能让您专注于算法和逻辑，而非底层平台细节。

### EGE 的优势

| 特点 | 说明 |
|------|------|
| 零依赖轻量级 | 使用 `stb_image` 和 `sdefl/sinfl` 替代 `libpng`/`zlib`，无外部依赖，单库即可使用 |
| 直接像素访问 | 提供 `getbuffer` 接口直接访问图像像素数据，实现高效的软件渲染和图像处理 |
| 抗锯齿支持 | 内置 GDI+ 支持，`ege_` 系列函数提供高质量抗锯齿绘图 |
| 预乘 Alpha 优化 | 默认使用 PRGB32 (预乘 Alpha) 格式，配合 `AlphaBlend` 实现 GPU 加速混合 |
| 多图像格式支持 | 支持 PNG, JPEG, BMP, GIF, TGA, PSD, HDR 等常见图像格式 |
| 灵活的图像操作 | 支持图像旋转、缩放、透明贴图、Alpha 滤镜等高级变换 |
| 坐标变换系统 | 提供 `ege_transform_*` 系列函数，支持平移、旋转、缩放等矩阵变换 |
| 完善的输入处理 | 支持键盘、鼠标（含双击、扩展键）、输入法等多种输入方式 |
| 相机捕获支持 | 基于 [ccap](https://github.com/wysaid/CameraCapture) 提供摄像头采集功能 (C++17) |
| 跨编译器兼容 | 从 VS2017 到 VS2026，MinGW 全系列均可编译，兼容性极强 |

## 构建指南

EGE 使用 [CMake](https://cmake.org) 作为构建系统。

详细编译步骤请参阅 [编译指南](BUILD.md)。

## 社区与支持

| 渠道 | 链接 |
|------|------|
| 官方网站 | <https://xege.org> |
| 源码仓库 | <https://github.com/x-ege/xege> |
| 社区论坛 | [EGE Club](https://club.xege.org) |
| 百度贴吧 | [ege](http://tieba.baidu.com/f?kw=ege)、[ege娘](http://tieba.baidu.com/f?kw=ege%C4%EF) |
| 教程博客 | [EGE 教程&介绍](https://blog.csdn.net/qq_39151563/article/details/100154767) (作者：[依稀](https://blog.csdn.net/qq_39151563?type=blog)) |
| 邮箱 | <this@xege.org> |
| QQ 群 | 1060223135 |

## 历史信息

<details>
<summary>原作者信息与历史链接（点击展开）</summary>

EGE 图形库最初由 [misakamm](https://github.com/misakamm/xege) 创建。以下为原作者的相关链接（部分已失效）：

| 资源 | 链接 | 状态 |
|------|------|------|
| 原仓库 1 | <http://misakamm.github.com/xege> | 已停止更新 |
| 原仓库 2 | <http://misakamm.bitbucket.org/index.htm> | 已停止更新 |
| Google+ 社区 | <https://plus.google.com/communities/103680008540979677071> | 已关闭 |
| 原作者博客 | <https://misakamm.com> | 无法访问 |
| 原作者邮箱 | <misakamm@gmail.com> | 无法联系 |

目前项目由社区维护，主仓库位于 <https://github.com/x-ege/xege>。

</details>
