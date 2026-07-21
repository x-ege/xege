# EGE 构建指南

EGE 使用 CMake 构建。日常开发优先使用仓库自带的 VS Code Tasks 或
`tasks.sh`；需要可复现的 CI、IDE 集成或自定义工具链时，直接使用标准 CMake
命令。

项目声明的最低 CMake 版本为 3.13；新环境建议使用当前稳定版。特定 generator
可能有更高要求，例如 Visual Studio 2026 需要 CMake 4.2 或更新版本。

测试的完整说明见 [`tests/README.md`](tests/README.md)。本文件只保留构建所需的
平台约定和推荐入口。

## 构建方式的选择

| 场景 | 推荐入口 |
| --- | --- |
| VS Code 本地开发 | `Terminal > Run Task...`，选择 `.vscode/tasks.json` 中的任务 |
| macOS/Linux 命令行开发 | `bash -l tasks.sh ...` |
| Windows 命令行开发 | 在 Git Bash 中运行 `bash -l tasks.sh ...` |
| CI、IDE 或自定义工具链 | 直接使用 `cmake -S`、`cmake --build` 和 `ctest` |
| 生成多套 Windows 发布库 | 按需使用 `build_commands.bat` 中与本机工具链对应的命令 |

`build_commands.bat` 是编译器兼容矩阵脚本，不是日常“一键构建”入口。它会顺序
尝试多代 Visual Studio、MinGW 和 VC6 命令，其中部分旧 generator 需要对应年代的
CMake 和工具链；不要在只安装了一个现代工具链的环境中直接运行整个文件。

## 快速开始

查看便捷脚本支持的参数：

```sh
bash -l tasks.sh --help
```

常用命令：

```sh
# Debug 库
# Windows GDI 与 OpenGL 使用彼此隔离的构建目录
bash -l tasks.sh --gdi --release --target demos --build
bash -l tasks.sh --opengl --release --target demos --build

bash -l tasks.sh --debug --target xege --build

# Debug 库和全部 demo
bash -l tasks.sh --debug --target demos --build

# Release 库和全部 demo
bash -l tasks.sh --release --target demos --build
```

`--gdi` 和 `--opengl` 会分别使用 `build/gdi` 和 `build/opengl`（单配置生成器还会
追加 Debug/Release），并设置对应的 CMake 后端选项；Windows OpenGL 模式同时
启用仓库内置 GLFW。未指定后端参数的旧命令保持原有目录和平台默认行为。

脚本会在构建目录尚未配置时自动运行 CMake。Windows 下必须从 Git Bash 或其他
POSIX shell 调用；直接使用 PowerShell/CMD 时请改用下文的标准 CMake 命令。

不要在同一构建目录中切换 generator、编译器、目标架构或
`EGE_BUILD_OPENGL`。为不同组合使用独立目录；需要重新配置现有脚本目录时，可用
对应的 `Reload CMake Project` VS Code Task 或 `tasks.sh --reload`。

## 平台默认值

| 主机平台 | 默认后端 | 默认 GLFW | 产物 |
| --- | --- | --- | --- |
| Windows | GDI | 不需要 | Windows `.exe` / `graphics.lib` |
| macOS | 原生 OpenGL | 仓库内置版本 | 原生可执行文件 / `libgraphics.a` |
| Linux | 原生 OpenGL | 仓库内置版本，默认 X11 | 原生可执行文件 / `libgraphics.a` |

Windows 可以用 `-DEGE_BUILD_OPENGL=ON` 编译 OpenGL 后端，但程序仍需通过
`INIT_OPENGL` 显式选择它。macOS/Linux 无需显式传入该选项。

## macOS 和 Linux

### 依赖

```sh
# macOS
xcode-select --install

# Ubuntu / Debian：bundled GLFW + X11
sudo apt-get update
sudo apt-get install cmake ninja-build xorg-dev libgl1-mesa-dev

# Arch Linux：bundled GLFW + X11
sudo pacman -S cmake ninja libx11 libxrandr libxinerama libxcursor libxi mesa
```

### 推荐的标准 CMake 构建

下面的命令不显式设置 OpenGL，用于遵循并验证 macOS/Linux 的默认原生后端：

```sh
cmake -S . -B build/native-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_TEMP=OFF

cmake --build build/native-debug --parallel
cmake --build build/native-debug --target demos --parallel
ctest --test-dir build/native-debug --output-on-failure -LE performance
```

Ninja、Unix Makefiles 等单配置 generator 在未指定 `CMAKE_BUILD_TYPE` 时，EGE
默认使用 `Release`。建议开发和 CI 仍显式指定配置，避免复用目录时产生歧义。

### 使用系统 GLFW

默认的 `EGE_USE_BUNDLED_GLFW=ON` 使用 `3rdparty/ccap` 内固定版本的 GLFW。
如需使用系统包：

```sh
# Ubuntu / Debian
sudo apt-get install libglfw3-dev

cmake -S . -B build/system-glfw -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEGE_USE_BUNDLED_GLFW=OFF \
  -DEGE_BUILD_TEMP=OFF
cmake --build build/system-glfw --parallel
```

macOS 可通过 Homebrew 安装 `glfw`，Arch Linux 可安装 `glfw`，然后使用相同的
CMake 选项。

### Linux Wayland

Bundled GLFW 在 Linux 默认只启用 X11。Wayland 是显式 opt-in，并且必须使用
独立构建目录：

```sh
sudo apt-get install libgl1-mesa-dev libwayland-dev libwayland-bin libxkbcommon-dev

cmake -S . -B build/wayland -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGLFW_BUILD_X11=OFF \
  -DGLFW_BUILD_WAYLAND=ON \
  -DEGE_BUILD_TEMP=OFF
cmake --build build/wayland --parallel
```

CI 会编译 Wayland 配置；自动化窗口运行测试仍以 X11 + Xvfb 为主。

## Windows

安装 Visual Studio 或 Visual Studio Build Tools 时，选择
[Desktop development with C++](https://learn.microsoft.com/cpp/build/vscpp-step-0-installation)
工作负载和 CMake 组件。直接运行 MSVC 命令时，使用 Developer PowerShell 或
Developer Command Prompt。

EGE 支持 Visual Studio 2017 至 2026。Visual Studio 2026 使用 `v145` 工具集和
`Visual Studio 18 2026` generator；该 generator 从
[CMake 4.2](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)
开始提供。使用 VS2026 时应安装 CMake 4.2 或更新版本。

当前 GitHub hosted Windows CI 持续验证 v141、v142 和 v143 工具集；VS2026/v145
已进入源码和本地构建脚本支持范围，但仍需要安装了 VS2026 的环境做实际验证。

### Visual Studio 2026 / MSVC

默认构建 Windows GDI 后端：

```powershell
cmake -S . -B build/vs2026 `
  -G "Visual Studio 18 2026" `
  -A x64 `
  -DEGE_BUILD_DEMO=ON `
  -DEGE_BUILD_TEST=ON `
  -DEGE_BUILD_TEMP=OFF

cmake --build build/vs2026 --config Debug --parallel
cmake --build build/vs2026 --config Debug --target demos --parallel
ctest --test-dir build/vs2026 -C Debug --output-on-failure -LE performance
```

构建 32 位库时把 `-A x64` 改成 `-A Win32`。使用 Visual Studio 2022 时把
generator 改成 `Visual Studio 17 2022`；也可以省略 `-G`，让 CMake 选择本机
默认 generator，但显式指定更适合可复现构建。

Visual Studio 是多配置 generator，配置阶段不要依赖 `CMAKE_BUILD_TYPE`，应在
构建和测试阶段使用 `--config Debug/Release` 与 `ctest -C Debug/Release`。MSVC
输出名称为：

- Debug：`graphicsd.lib`
- Release、RelWithDebInfo、MinSizeRel：`graphics.lib`

### Windows OpenGL（显式开启）

```powershell
cmake -S . -B build/windows-opengl `
  -A x64 `
  -DEGE_BUILD_OPENGL=ON `
  -DEGE_USE_BUNDLED_GLFW=ON `
  -DEGE_BUILD_DEMO=ON `
  -DEGE_BUILD_TEST=ON `
  -DEGE_BUILD_TEMP=OFF

cmake --build build/windows-opengl --config Release --parallel
```

这会把 GDI 和 OpenGL 能力编入 Windows 库；未传 `INIT_OPENGL` 的现有程序仍走
GDI，不改变默认行为。

GitHub 托管的 Windows runner 没有可用的 WGL 驱动，因此常规
`native-opengl-build.yml` 会编译全部 OpenGL 用例，但只运行默认 GDI 兼容用例。
仓库另有可手动触发的 `windows-opengl-runtime.yml`：它要求带
`self-hosted`、`windows`、`x64`、`opengl` 标签且具有交互式桌面和 OpenGL 3.3
驱动的 runner，并在那里运行 GDI/OpenGL 全部功能与性能门禁。

### MinGW-w64

在 MSYS2/MinGW shell 中优先使用其自带的现代 GCC、CMake 和 Ninja。也可以使用
`MinGW Makefiles`：

```sh
cmake -S . -B build/mingw -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DEGE_BUILD_OPENGL=OFF \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_TEMP=OFF
cmake --build build/mingw --parallel
ctest --test-dir build/mingw --output-on-failure -LE performance
```

VC6、Visual Studio 2010–2015 和旧版 MinGW 仅保留兼容性用途，不作为现代开发
环境推荐。它们通常需要匹配的历史 CMake/toolchain；相应命令仍可在
`build_commands.bat` 中查阅。

## Demo、测试和临时程序

构建全部 demo：

```sh
cmake --build <build-dir> --target demos --parallel
```

多配置 generator 需要附加 `--config Debug` 或 `--config Release`。原生
macOS/Linux demo 没有 `.exe` 后缀；`tasks.sh` 会兼容 VS Code Tasks 中沿用的
`.exe` 名称并映射到原生文件。

功能测试、性能测试、Sanitizer、相机 fixture 和 Linux Xvfb 的标准命令统一维护在
[`tests/README.md`](tests/README.md)，不要从旧构建目录推断测试结果。

临时实验可以放在被 Git 忽略的 `temp/` 目录，并提供自己的 `CMakeLists.txt`。
根项目会在 `EGE_BUILD_TEMP=ON` 且目录存在时添加它；正式回归应放在 `tests/`。

## 主要 CMake 选项

| 选项 | Windows 默认值 | macOS/Linux 默认值 | 说明 |
| --- | --- | --- | --- |
| `EGE_BUILD_OPENGL` | `OFF` | `ON` | 选择要编译的图形后端能力 |
| `EGE_USE_BUNDLED_GLFW` | `OFF` | `ON` | 使用 `ccap` 内固定版本的 GLFW |
| `EGE_BUILD_DEMO` | 顶层构建为 `ON` | 顶层构建为 `ON` | 添加 demo 目标 |
| `EGE_BUILD_TEST` | 顶层构建为 `ON` | 顶层构建为 `ON` | 添加 CTest 测试 |
| `EGE_BUILD_TEMP` | 顶层构建为 `ON` | 顶层构建为 `ON` | 添加本地 `temp/` 实验目标 |
| `EGE_ENABLE_CAMERA_CAPTURE` | C++17 可用时 `ON` | C++17 可用时 `ON` | 构建相机模块 |
| `EGE_PERFORMANCE_DIAGNOSTICS` | `AUTO` | `AUTO` | `AUTO` 仅在 Debug 编译性能诊断；也可显式设为 `ON` 或 `OFF` |
| `EGE_DISABLE_DEBUG_INFO` | `OFF` | `OFF` | 禁用调试信息，主要用于规避特定 MSVC 链接警告 |

`EGE_BUILD_FOR_LINUX` 是由平台和 `EGE_BUILD_OPENGL` 推导的内部兼容选项，不应
由新构建手动设置。

### 性能诊断

Debug 默认会对可确认的 OpenGL 像素同步慢路径输出一次性诊断；Release 默认不编译诊断
热路径。运行程序前设置 `EGE_DIAGNOSTICS=log` 可保留日志但关闭 Windows 非模态提示窗，
设置 `EGE_DIAGNOSTICS=off` 可完全静默；`NO_COLOR` 禁止终端颜色。完整编号、触发条件和
行为分级见 [`doc/performance-diagnostics.md`](doc/performance-diagnostics.md)。

## 运行问题排查

- Linux X11 窗口需要可用的 `DISPLAY` 和 OpenGL/GLX。Headless 环境使用 Xvfb，
  命令见 `tests/README.md`。
- Wayland 构建需要真实 Wayland 会话才能运行窗口程序；CI 当前只保证该配置可编译。
- macOS 图形程序需要 GUI session。部分 hosted runner 没有可用的 NSGL 环境，因此
  CI 会把编译门禁与无需窗口的确定性测试分开。
- 如果 bundled GLFW/ccap 缺失且 CMake 无法自动同步，再运行
  `git submodule update --init --recursive`，不要把它作为每次构建步骤。
- Unix 上显式设置 `-DEGE_BUILD_OPENGL=OFF` 会进入已弃用的 mingw-w64 → Windows
  `.exe` 交叉编译路径。仅在维护旧流程时使用，并为它创建独立构建目录；运行产物还
  需要 Wine。
