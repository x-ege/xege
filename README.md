# EGE (Easy Graphics Engine)

[![MSVC Build](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml)
[![MinGW Build](https://github.com/x-ege/xege/actions/workflows/mingw-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/mingw-build.yml)
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
- **灵活绘图** — 绘图可直接针对图像或控件进行，不限于屏幕
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

EGE 支持以下 IDE 和编译器：

| 编译器/IDE | 版本支持 |
|------------|----------|
| Visual C++ | 6.0 |
| Visual Studio | 2008 ~ 2022 |
| MinGW / MinGW-w64 | 支持 |
| Dev-C++ | 支持 |
| Code::Blocks | 支持 |
| 小熊猫 C++ | 支持 |
| CLion | 支持 |
| Eclipse for C/C++ | 支持 |

## 文档与资源

- **API 文档**：[API 参考](man/api.md)
- **详细帮助**：`man` 目录下的 `index.htm`

## 为什么选择 EGE

许多学习编程的人从 C 语言开始入门，但现有的教学环境存在一些问题：

1. **Turbo C 环境过时** — DOS 环境与现代操作系统兼容性差，可用颜色有限，编辑功能不便
2. **Windows 图形编程门槛高** — 在 Visual Studio 中绘制简单图形需要处理窗口类、消息循环等复杂概念
3. **图形学教学偏离重点** — 计算机图形学课程应聚焦于算法而非平台编程，但 Windows GDI 或 OpenGL 的门槛较高

**EGE 的解决方案**：无论您是 C 语言初学者、编程教师，还是计算机图形学讲师，EGE 都能让您专注于算法和逻辑，而非底层平台细节。

### EGE 的优势

| 特点 | 说明 |
|------|------|
| 高效渲染 | 窗口锁定模式下可直接使用 `getpixel`/`putpixel` 进行高效像素操作 |
| 灵活绘图 | 支持在图像对象或控件上绘图，不限于屏幕 |
| 丰富功能 | 图片旋转、透明贴图、滤镜、多格式支持、精确帧率控制 |
| 开源免费 | 完全开源，欢迎贡献代码 |

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
