# EGE 跨平台支持 / Cross-Platform Support

## 概述 / Overview

EGE 现在支持多渲染后端架构，允许在保持向后兼容的同时，支持跨平台渲染。

EGE now supports a multi-backend rendering architecture, allowing cross-platform rendering while maintaining backward compatibility.

## 渲染后端 / Rendering Backends

### GDI 后端 (默认) / GDI Backend (Default)

- **平台 / Platform**: Windows only
- **状态 / Status**: 稳定 / Stable
- **说明 / Description**: 使用 Windows GDI/GDI+ 进行渲染，这是 EGE 的原始实现。

### OpenGL 后端 (实验性) / OpenGL Backend (Experimental)

- **平台 / Platform**: Windows, macOS, Linux
- **状态 / Status**: 实验性 / Experimental
- **说明 / Description**: 使用 OpenGL 进行渲染，通过 GLFW 管理窗口，实现真正的跨平台支持。

## 使用方法 / Usage

### 编译时启用 OpenGL / Enable OpenGL at Build Time

```bash
# CMake 配置时启用 OpenGL 后端
cmake .. -DEGE_ENABLE_OPENGL=ON
```

### 运行时选择后端 / Select Backend at Runtime

```cpp
#include <ege.h>

int main() {
    // 使用默认 GDI 后端 / Use default GDI backend
    ege::initgraph(640, 480, ege::INIT_DEFAULT);

    // 或使用 OpenGL 后端 / Or use OpenGL backend
    ege::initgraph(640, 480, ege::INIT_OPENGL);

    // ... 绑定代码相同 / binding code is the same
}
```

## 后端抽象层 / Backend Abstraction Layer

### 目录结构 / Directory Structure

```
src/
└── backend/
    ├── render_backend.h      # 抽象接口 / Abstract interface
    ├── render_backend.cpp    # 后端工厂 / Backend factory
    ├── gdi_backend.h         # GDI 后端头文件
    ├── gdi_backend.cpp       # GDI 后端实现
    ├── opengl_backend.h      # OpenGL 后端头文件
    └── opengl_backend.cpp    # OpenGL 后端实现
```

### RenderBackend 接口 / Interface

所有渲染后端都实现 `RenderBackend` 接口，提供以下功能：

All rendering backends implement the `RenderBackend` interface, providing:

- 初始化和关闭 / Initialization and shutdown
- 缓冲区交换 / Buffer swapping
- 清屏 / Clear screen
- 像素操作 / Pixel operations
- 基本图形绘制（线、矩形、圆、椭圆）/ Basic shape drawing (line, rectangle, circle, ellipse)
- 事件处理 / Event processing

## 依赖 / Dependencies

### OpenGL 后端依赖 / OpenGL Backend Dependencies

- **GLFW**: 窗口管理 / Window management
- **GLAD**: OpenGL 加载器 / OpenGL loader

这些依赖可以通过以下方式添加 / These dependencies can be added via:

1. 系统包管理器 / System package manager
2. Git 子模块到 `3rdparty/` / Git submodules to `3rdparty/`

## 已知限制 / Known Limitations

OpenGL 后端目前处于实验阶段，以下功能可能不完全支持：

The OpenGL backend is currently experimental, the following features may not be fully supported:

1. 文字渲染 / Text rendering
2. 图像操作 / Image operations
3. 部分高级 GDI+ 功能 / Some advanced GDI+ features

## 路线图 / Roadmap

1. **阶段 1** (已完成): 添加 `INIT_OPENGL` 标志和后端抽象层
2. **阶段 2**: 集成 GLFW 和 GLAD
3. **阶段 3**: 实现基本绘图功能的 OpenGL 版本
4. **阶段 4**: 添加文字渲染支持
5. **阶段 5**: 添加图像操作支持

---

## 开发者指南 / Developer Guide

### 添加新后端 / Adding a New Backend

1. 在 `src/backend/` 创建新的后端类
2. 继承 `RenderBackend` 接口
3. 实现所有虚函数
4. 在 `render_backend.cpp` 中注册后端
5. 更新 CMakeLists.txt 添加条件编译

### 测试后端 / Testing a Backend

使用 `demo/graph_opengl_test.cpp` 作为测试参考。

Use `demo/graph_opengl_test.cpp` as a testing reference.
