# XEGE 构建目标与后端

CMake 现在把“目标平台”和“绘制后端”作为两个独立概念：宿主机是
macOS/Linux 不再自动切换为 Windows 交叉编译。

## 主要选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `EGE_BUILD_DEMO` | 根项目 ON | 生成 demo 目标 |
| `EGE_BUILD_TEST` | 根项目 ON | 注册目标平台的 CTest 套件 |
| `EGE_ENABLE_WINDOW_TESTS` | OFF | 注册会显示窗口的 macOS 测试；不应在 headless CI 开启 |
| `EGE_BUILD_TEMP` | 根项目 ON | 构建 gitignored `temp/` 中的本地程序 |
| `EGE_DEFAULT_BACKEND` | `AUTO` | Windows→`GDI`，macOS→`COREGRAPHICS`；Linux `CAIRO` 尚未实现 |
| `EGE_ENABLE_OPENGL` | OFF | 实验后端开关；当前源码不完整时 fail-fast |
| `EGE_ENABLE_CAMERA_CAPTURE` | 动态 | C++17 且 ccap 子模块完整时 ON，否则 OFF |
| `EGE_ENABLE_CPP17` | 编译器探测 | 启用 C++17 内部路径 |
| `EGE_DISABLE_DEBUG_INFO` | OFF | MSVC 下禁用调试信息，用于特定发布兼容场景 |
| `EGE_CLEAR_OPTIONS_CACHE` | OFF | 一次性清除上述 EGE 选项缓存并重新探测 |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | macOS 原生为 `11.0` | 可由调用者显式覆盖；正式制品固定 11.0 |

## macOS 原生构建

```sh
cmake -S . -B build/native-coregraphics \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DEGE_DEFAULT_BACKEND=COREGRAPHICS \
  -DEGE_ENABLE_OPENGL=OFF \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEMP=OFF
cmake --build build/native-coregraphics --parallel
cmake --build build/native-coregraphics --target graph_5star --parallel
cmake -E chdir build/native-coregraphics ctest --output-on-failure
file build/native-coregraphics/demo/graph_5star
```

最后一条命令应识别出 `Mach-O`。这是 AppleClang + AppKit/Core Graphics 的原生
产物，不是依赖 Wine 运行的 Windows `.exe`。

`EGE_DEFAULT_BACKEND=AUTO` 的解析规则是：Windows 使用 `GDI`，macOS
使用 `COREGRAPHICS`，Linux 使用 `CAIRO`。也可以显式传入
`GDI|COREGRAPHICS|CAIRO|OPENGL`；平台与后端不兼容时，配置阶段会直接报错。
当前 Linux Cairo 实现源码尚未接入，因此 Linux 原生 `AUTO`/`CAIRO` 会明确
fail-fast；不会隐式改为 MinGW 并生成 `.exe`。

OpenGL 不是原生构建的前提。只有明确传入 `-DEGE_ENABLE_OPENGL=ON` 时才会
查找 OpenGL 和 GLFW；把默认后端设为 `OPENGL` 时也必须同时启用该选项。
旧 OpenGL 分支只作为 CMake 分层与后端接口的参考；它不是默认后端，也不是
Core Graphics 构建依赖。当源码树没有 OpenGL 实现时，显式启用会 fail-fast。

## macOS 像素内存契约

Core Graphics 后端将 CPU `PixelSurface` 作为权威像素存储。内存为顶向下、紧密排列
的预乘 BGRA，小端数值为 `0xAARRGGBB`，stride 等于 `width * sizeof(color_t)`。
`getbuffer()` 直接返回这块 CPU 内存，Core Graphics 也直接绘制到同一块内存，因而
逐像素读写不需要 GPU readback。在 `IMAGE` 重建、resize 或销毁后，应重新获取
指针。

## Windows 交叉编译

交叉编译必须在第一次配置时显式选择 toolchain：

```sh
cmake -S . -B build/mingw \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DEGE_DEFAULT_BACKEND=GDI
cmake --build build/mingw --config Release
```

可通过 `EGE_MINGW_TRIPLE` 选择其他 mingw-w64 target triple，通过
`EGE_MINGW_ROOT` 指定可选 sysroot。不要在已配置为原生构建的 build 目录中
切换 toolchain。

## 子模块

CMake configure 不会运行 Git 或修改源码目录。需要 camera 时，请先执行：

```sh
git submodule update --init --recursive 3rdparty/ccap
```

未初始化 ccap 时，camera 默认关闭；如果显式设置
`EGE_ENABLE_CAMERA_CAPTURE=ON`，配置会给出可操作的错误信息。
