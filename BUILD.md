# EGE 编译指南

EGE 源码使用 CMake 3.13 或更高版本构建。Windows 默认使用 GDI；macOS 现在可用
AppleClang 直接生成 Mach-O 原生程序，默认绘制后端为 Core Graphics，窗口后端为
AppKit。macOS 原生构建不需要 MinGW、Wine 或 OpenGL。

EGE 的可选子模块由 Git submodule 管理。CMake 配置阶段不会访问网络或修改
源码目录。默认关闭 camera 时不需要拉取 `ccap`；如果需要 camera，请在配置前执行：

```sh
git submodule update --init --recursive 3rdparty/ccap
```

请安装 CMake 和目标平台的原生编译器。macOS 使用 Xcode Command Line Tools；Windows
使用 MSVC 或 MinGW-w64。Linux 主机仍可通过显式 toolchain 交叉编译 Windows 版，但
Linux 原生 Cairo 后端尚未实现，配置会 fail-fast，不会默认生成依赖 Wine 的 `.exe`。

## macOS 原生 Core Graphics 构建

安装 Xcode Command Line Tools 后，在仓库根目录执行：

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

`file` 的最后一行应包含 `Mach-O`，而不是 `PE32` 或 `.exe`。`COREGRAPHICS` 是 macOS
的默认后端，但建议 CI 与验收脚本显式指定，避免缓存污染。
macOS 11.0 是当前支持下限。根项目和发布工作流会固定该值，
以免滚动 Xcode SDK 将制品错标成只支持构建机的新系统。

默认 CTest 严格使用无头模式，不创建 `NSApplication` 或 `NSWindow`。公共 API 测试通过
`EGE_HEADLESS=1` 初始化全局画布，将结果写入
`build/native-coregraphics/test-artifacts/ege-api-headless.png`，再解码并逐像素验证。
会显示真实窗口的 `native.mac_window_smoke`、
`native.public_close_callback_contract` 和 `demo.*.launch` 不会注册到默认测试集；
只有人工明确配置 `-DEGE_ENABLE_WINDOW_TESTS=ON` 时才启用。
`native.ege_music_contract` 会打开自动生成的静音 WAV 并测试损坏输入，
不做可听播放；camera 目标在默认 CI 中只编译/链接，不访问真实设备。

### VS Code 与 `tasks.sh`

仓库自带的 VS Code 任务统一调用 `tasks.sh`。在 macOS 上，脚本会显式配置
`EGE_DEFAULT_BACKEND=COREGRAPHICS` 和 `EGE_ENABLE_OPENGL=OFF`，并使用独立的
`build/macos/Debug`、`build/macos/Release` 目录，避免复用旧 MinGW CMake cache。
运行 demo 时传入的是无扩展名的 CMake target 名称，脚本直接启动 Mach-O 文件，
不会查找 `.exe` 或调用 Wine。只有显式传入 Windows toolchain 时才进入交叉编译路径。

可以先用下面的只读命令检查任务将使用的配置：

```sh
bash tasks.sh --debug --show-config
```

`tasks.sh --clean` 和 `--reload` 会删除当前选中的 CMake build 目录。
脚本会拒绝删除仓库根目录或无法确认属于本项目的自定义目录，
但调用前仍应检查 `--show-config` 输出。

`utils/release.sh` 在 macOS 上生成的 AppleClang 静态库位于
`Release/lib/macOS`。脚本分别构建 arm64/x86_64，用 `lipo` 合成 universal
archive，检查每个 slice 的 macOS 11.0 标记，并使用与官方包相同的
`.github/release-assets/CMakeLists.txt` 链接所有 demo。构建中间件默认位于
`build/release-local`，可用 `EGE_RELEASE_BUILD_ROOT` 改变；测试或自动化可用
`EGE_RELEASE_DIR` 把输出定向到隔离目录。脚本会启用 camera，因此必须先初始化
`3rdparty/ccap`。`EGE_MACOS_DEPLOYMENT_TARGET` 可覆盖本地试验目标，但正式发布与
兼容性验收固定为 11.0。

仓库和发布包中的 camera demo 已内嵌 `NSCameraUsageDescription`。自行创建的 macOS
camera 可执行文件或 app bundle 也必须在其 Info.plist 中提供该键，否则系统可能在
首次访问摄像头时终止程序；同时仍需由用户授予相机权限。

Windows 专用的 `utils/release-msvc.sh`、`utils/release-mingw.sh` 和
`utils/test-release-libs.sh` 不会隐式执行 `git clean`。前两个脚本默认使用按工具集/架构
隔离的构建目录，仅在显式传入 `--force-clean` 时删除仓库内准确的 `build/` 与
`Release/` 目录；调用前请先确认其中没有需要保留的生成物。`--help` 只显示帮助，
不修改工作树。VS2010 构建需要临时为
部分源码添加 BOM，脚本通过退出 trap 恢复编码；仍建议在专用发布工作树中运行。
`utils/release.sh` 的 macOS 路径也不清理工作树，并可用上述环境变量完全隔离输出。

`utils/test-run-demos.sh --directory <demo-build-dir>` 可在交互式桌面会话中启动
已构建 demo：macOS 直接运行 Mach-O，Linux 对 Windows `.exe` 明确使用 Wine。
camera demo 默认排除，只有显式传 `--include-camera` 才会触发其权限/设备路径。

### 逐像素 CPU buffer 语义

macOS 后端以 CPU `PixelSurface` 作为图像像素的权威存储。`getbuffer()` 返回可直接
读写的、从上到下排列的连续像素：每行是 `width * sizeof(color_t)` 字节，在小端
macOS 上数值表示为 `0xAARRGGBB`，内存字节顺序为预乘 Alpha 的 BGRA。Core Graphics
直接绘制到同一块 CPU buffer，所以常规逐像素读写不发生 GPU readback 或 CPU/GPU
双向同步。指针在对应 `IMAGE` 不重建、不缩放且不销毁期间有效。

OpenGL 实验分支只作为 CMake 分层和后端接口的参考，不作为原生构建的默认依赖。
`EGE_ENABLE_OPENGL` 默认为 `OFF`；本分支未携带 OpenGL 实现源码时，显式开启也会
fail-fast。

## Windows 快速编译

windows端可以通过运行根目录下的`build_commands.bat`批处理脚本快速在windows下编译代码，
此代码会尝试检查计算机中是否有配置好的MinGW、Visual Studio 2022等环境，并尝试编译，
编译得到的文件位于新生成的`build`文件夹下，其中`build/lib`中会分别存放好编译得到的静态库。

具体而言，目前支持快速编译的列表如下：

 - MinGW
 - Visual Studio 2008
 - Visual Studio 2010
 - Visual Studio 2012
 - Visual Studio 2013
 - Visual Studio 2015
 - Visual Studio 2017
 - Visual Studio 2019
 - Visual Studio 2022

详细信息请检查脚本具体内容。

## 在 Linux 上交叉编译 Windows 版

```sh
# Ubuntu 16.04及以上发行版
sudo apt-get install mingw-w64 wine

# Arch Linux
sudo pacman -S mingw-w64 wine
```

Linux 原生后端尚未完成，因此 Linux 发布和 CI 继续通过
`cmake/toolchains/mingw-w64.cmake` 显式生成 Windows 静态库及 `.exe`。macOS
不再支持或发布这条交叉编译路径。

## 基本编译步骤

1. 创建 build 文件夹并设为当前目录

  ```sh
  mkdir build
  cd build
  ```

2. 执行 `cmake` 命令生成编译配置文件

  ```sh
  cmake .. [编译配置]
  ```

  `[编译配置]` 指定特定的编译平台，将在后文详述。

3. 进行编译

  ```sh
  cmake --build .
  ```

编译过程将在 `build` 目录下生成相应的静态库文件。

如果想在完成编译后使用其它编译器再次编译，请先清空 `build` 目录，CMD 命令为

```sh
for /D %d in (*) do @rmdir %d /S /Q
del * /S /Q
```

清空目录后再从步骤 2 继续执行。

## 编译配置

不同编译平台的差别主要是配置编译步骤的第二步。

### MinGW

```sh
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```

CMake 会自动检测安装的 MinGW 编译器并生成编译配置。

编译配置中的 `-DCMAKE_BUILD_TYPE=Release` 是构建类型（Build type），表示生成优化级别较高的
发布版。

如果想指定编译套件（如 Dev-C++ 自带的 TDM-GCC 4.9.2），可在执行 `cmake`
前设置 `PATH` 环境变量指向特定的 MinGW 所在位置，CMake 在配置时会采用最先
在 PATH 中检测到的编译器进行编译。例如，Dev-C++ 安装目录为 `C:\Dev-Cpp`，则
在 CMD 中执行以下命令：

```cmd
set PATH="C:\Dev-Cpp\MinGW64\bin";%PATH%
```

在 PowerShell 中则是：

```ps
$env:PATH="C:\Dev-Cpp\MinGW64\bin;$env:PATH"
```

注意，CodeBlocks 附带的 MinGW 只能在
[MSYS Makefiles 配置](#msys-makefiles-配置)
下编译。在此建议您下载不附带 MinGW 的 CodeBlocks 并单独安装最新版 TDM-GCC64，
CodeBlocks 会自动识别已安装的 TDM-GCC。

#### 使用 64 位 MinGW 编译 32 位 EGE 库

以上步骤对 32 位与 64 位 MinGW 均适用，分别会产生对应 64 位和 32 位版本的 EGE 静态库。

64 位 MinGW 支持编译 32 位目标，要想达到这一效果，在以上步骤中设置 `PATH` 环境变量
之后，执行 `cmake` 之前需要设置 `CC` 和 `CXX` 环境变量。在 CMD 中命令为：

```cmd
set CC="gcc -m32"
set CXX="g++ -m32"
```

在 PowerShell 中为：

```ps
$env:CC="gcc -m32"
$env:CXX="g++ -m32"
```

之后再执行 `cmake .. -G "MinGW Makefiles"` 命令。

#### MSYS Makefiles 配置

如果您在使用 MSYS2 或 git-bash，您可以用 `MSYS Makefiles` 生成适合此类环境的编译系统。

所需要的命令和上面描述的没有区别，但需要把 CMD 或 PowerShell 命令换成 Bash 命令，
比如设置环境变量：

```sh
export PATH=/C/Dev-Cpp/MinGW64/bin:$PATH
```

相应的 CMake 生成指令是

```sh
cmake .. -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
```

注意，您需要从 `pacman` 包管理器安装或者
[相关网站](https://sourceforge.net/projects/ezwinports/files/)
上下载 MSYS make 程序并将其复制到可执行路径如 `/usr/bin` 中。

扩展阅读：[Windows 上的编译系统](https://www.chirsz.cc/blog/2020-03/compile-sys-on-win.html)

### Visual C++ 6.0

CMake 自 3.6 后停止了对生成 VC6 项目的支持，但仍可生成 NMake 编译系统以支持 VC6 编译。

首先，检查你的 VC6 安装目录（以下用 `VC6PATH` 指称，如果是免安装版则对应解压出的
`vc6` 文件夹的路径，这个路径应包含 `VC98` 和 `Common` 文件夹），在
`VC6PATH\VC98\Bin` 文件夹中的 `VCVARS32.BAT` 文件中开始部分的内容应和 `VC6PATH`
相一致，例如安装到 `D:\VC6` 的 VC6，在 `D:\VC6\VC98\Bin\VCVARS32.BAT` 中的开头部分
应当是：

```bat
rem Root of Visual Developer Studio installed files.
rem
set MSDevDir=D:\VC6\Common\msdev98

rem
rem Root of Visual C++ installed files.
rem
set MSVCDir=D:\VC6\VC98
```

不一致的情况常出现于免安装版 VC6，此时需修改 `VCVARS32.BAT` 内容使其与 VC6 实际所在
目录一致。

确认 `VCVARS32.BAT` 内容正确后，在 EGE 源码的 `src\build` 目录下执行此批处理文件，
在上面的例子中就是执行：

```sh
"D:\VC6\VC98\Bin\VCVARS32.BAT"
```

执行成功后即建立 VC6 命令行环境，就可以继续执行编译步骤二了：

```sh
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
```

编译配置中的 `-DCMAKE_BUILD_TYPE=Release` 是构建类型（Build type），表示生成优化级别较高的
发布版。

如果编译的源文件中使用 UTF-8 字符串字面量, 那么在之后的编译阶段需要临时修改 locale 来让 VC6 能够
处理 UTF-8 字符串。相关处理已经位于 `utils\patch-locale.bat` 脚本中, 使用此脚本执行编译命令即可。
例如编译示例程序:

```bat
.\utils\patch-locale.bat cmake --build build --target demos
```

### Visual Studio

使用 `cmake -G` 命令查看支持的 Visual Studio 版本，选择自己安装的 VS 版本，
如“Visual Studio 14 2015”作为 `-G` 的参数传递给 CMake。可用 `-A` 参数指定
目标平台，VS2017 及更早的 Visual Studio 默认为 32 位 x86 平台 `Win32`，
VS2019 则与所在平台一致。可选的参数有 `Win32`，`x64`，`ARM`，`ARM64`。

因此编译 32 位 EGE 库需要执行：

```sh
cmake .. -G "Visual Studio 14 2015" -A Win32
```

编译 64 位 EGE 库：

```sh
cmake .. -G "Visual Studio 14 2015" -A x64
```

另外要注意的一点是，Visual Studio 的步骤三应该是：

```sh
cmake --build . --config Release
cmake --build . --config Debug
```

因为 Visual Studio 是多配置的编译系统，需要在执行编译时选择配置类型（Configuration type）。
EGE 现在为 MSVC 提供了 Debug 和 Release 两个版本的静态库：
- `graphics.lib` - Release 版本
- `graphicsd.lib` - Debug 版本

头文件 `ege.h` 会根据 `_DEBUG` 宏自动选择链接正确的库文件。建议同时编译 Debug 和 Release 两个版本，
以便用户在不同配置下使用。

## 编译示例程序

示例程序是一批来自社区贡献的 EGE 项目，用于演示和测试，其多数为单文件项目，源代码位于 `demo` 目录下。

项目 CMake 配置中设置了 `demos` 目标，编译时通过 `--target` 或 `-t` 指定，即在编译项目时执行：

```sh
cmake --build . --target demos
```

即可在构建目录下的 `demo` 文件夹中生成各可执行文件。

`graph_rotateimage` 的 Windows 资源文件已使用跨工具链可识别的路径，
无需在 Linux MinGW 交叉编译前修改源文件。

## 编译临时测试文件

有时候为了测试新添加的功能，需要写一些测试用例，但编译安装修改后 EGE 再在项目外编译测试程序
比较麻烦，在项目里修改 CMake 配置并添加源文件和编译指令又会被 git 识别为未暂存的修改，会对 git
使用造成干扰。

项目 CMake 配置中已经写好了，开发者可以新建 `temp` 目录并添加源文件和 `CMakeLists.txt` 配置
文件，编译系统会自动配置，在编译 EGE 库后编译 `temp` 目录，而 temp 目录已被 `.gitignore`
排除在外，不会对 git 使用造成干扰。

例如，我们想要测试读取字符输入，于是新建 `temp` 目录，在 `temp` 目录下
新建 `CMakeLists.txt` 内容如下：

```cmake
add_executable(temp_test temp_test.cpp)

target_link_libraries(temp_test xege)

```

新建 `temp/temp_test.cpp` 内容如下：

```cpp
#include <ege.h>

using namespace ege;

int main(int argc, char const *argv[])
{
  initgraph(640, 480);

  circle(120, 120, 100);

  int i = 0;
  while (is_run()) {
    key_msg msg = getkey();
    if (msg.msg == key_msg_char) {
      xyprintf(0, i * 20, "%d", msg.key);
      ++i;
    }
  }

  return 0;
}

```

执行前文所述编译步骤后会在 build 目录的 `temp/` 下生成可执行文件：
Windows 为 `temp_test.exe`，macOS 为无扩展名的 `temp_test`。

## Linux 主机上交叉编译 Windows 例程

本节说明的是在 Linux 主机上生成 Windows `.exe`，不是 Linux 原生构建。编译这类
依赖 EGE 的程序需使用 `mingw-w64` 工具链中的 `g++`，并且根据
环境可能需要添加额外的编译参数 `-D_FORTIFY_SOURCE=0`
（参考链接 [undefined reference to `__memcpy_chk'](https://github.com/msys2/MINGW-packages/issues/5868)。
为了简化单文件编译指令，EGE `utils` 目录下提供了`ege_g++.sh` 脚本，可按需使用。
