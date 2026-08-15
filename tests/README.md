# EGE 测试说明

仓库包含两组不同目标的测试，不要把“编译通过”、“无界面运行”和
“真实窗口/硬件验收”视为同一件事。

## macOS 原生契约测试

`tests/native/` 覆盖 Core Graphics/AppKit 后端，完整分类见
[TEST_MATRIX.md](native/TEST_MATRIX.md)。默认套件为 headless，不创建窗口、
不请求摄像头权限，也不播放声音。

```sh
cmake -S . -B build/mac-tests \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DEGE_DEFAULT_BACKEND=COREGRAPHICS \
  -DEGE_ENABLE_OPENGL=OFF \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEMP=OFF \
  -DEGE_ENABLE_WINDOW_TESTS=OFF
cmake --build build/mac-tests --parallel
cmake -E chdir build/mac-tests ctest --output-on-failure
```

其中包括公开中/英文头文件、EGE/AppKit 在 Objective-C++ 中的两种 include
顺序、像素/基本图元、图像编解码、文本、增强
API、控件焦点、`MUSIC` 文件生命周期、进程退出码和 CMake/发布契约。
生成的 PNG/BMP/WAV 位于 `build/mac-tests/test-artifacts/`。

### 可见窗口测试（显式 opt-in）

```sh
cmake -S . -B build/mac-window-tests \
  -DCMAKE_BUILD_TYPE=Release \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF \
  -DEGE_ENABLE_WINDOW_TESTS=ON
cmake --build build/mac-window-tests --parallel
cmake -E chdir build/mac-window-tests ctest --output-on-failure \
  -R 'native\.(mac_window_smoke|public_close_callback_contract)|demo\..*\.launch'
```

这组测试会短暂显示 AppKit 窗口，需要登录的 WindowServer 会话。无 GUI
会话时返回 77 并标记为 `Skipped`；`Skipped` 不是运行通过。camera demo
仅做 build/link，不注册到该套件。

## Windows putimage/性能套件

`tests/tests/` 是历史 Win32 GDI 性能/运行烟雾套件，直接链接 Windows 系统
库，因此只在 Windows 目标上加入构建。它覆盖 `putimage*`、Alpha/透明色和
旋转等场景，但主要输出计时结果，没有稳定的像素正确性断言，不作为 CI
正确性门禁。

```powershell
cmake -S . -B build/windows-tests -DEGE_BUILD_TEST=ON -DEGE_BUILD_DEMO=OFF
cmake --build build/windows-tests --config Release --parallel
cmake -E chdir build/windows-tests ctest -C Release --output-on-failure -L performance
```

可执行文件通常位于 `build/windows-tests/bin/Release/`（具体取决于生成器）。
性能结果会受机器负载、调试/发布配置和窗口会话影响，不应作为跨机器
稳定性结论。Windows CI 使用 `-LE performance`，只运行有确定结果的 CMake
契约；性能套件保留为人工运行入口。

## 覆盖边界

- camera：CI 和默认 CTest 只证明源码编译/链接；真实设备、TCC 权限和长时运行需人工验收。
- audio：自动测试只打开生成的静音 WAV 并校验状态/错误路径，不依赖物理输出设备。
- x86_64 macOS：发布 CI 会编译并链接 archive slice，但不代表已在 Intel Mac 上运行。
- Linux：当前只有显式 MinGW Windows 交叉编译，没有原生图形后端测试。
