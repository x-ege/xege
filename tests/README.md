# EGE 测试套件

测试套件同时覆盖 EGE 的行为正确性以及贴图、像素缓冲性能。跨平台重构以确定性的像素、状态和文件往返断言为主，不依赖人工观察窗口截图。所有计时程序都带有 `performance` 标签，不作为每个平台/配置都重复执行的功能门禁。

## 覆盖范围

| 测试 | 主要覆盖内容 |
| --- | --- |
| `default_build_contract` | 从空目录重新配置，验证 Windows 默认 GDI、Linux/macOS 默认 OpenGL、单配置生成器默认 Release，以及 Linux bundled GLFW 的 X11/Wayland 默认值 |
| `rendering_correctness` | 基础与增强图元、线型/端帽/连接、填充与渐变、viewport、路径、变换、文字与编码、图片混合/旋转、像素缓冲与存储转换、压缩、颜色工具、PNG/BMP 保存与重新加载 |
| `public_headers` | 公共头文件、Win32 兼容类型及常量的可编译性 |
| `public_api_overloads` | 让编译器逐一选择两个公共头中全部 386 个精确函数签名，防止同参数个数的类型重载被静态审计误判为覆盖 |
| `music_contract` | 未打开/缺失文件错误；Unix PCM WAV 的打开、时长、定位、区间播放、暂停、区间循环、音量、停止、窄/宽路径和幂等关闭；Linux GStreamer 与 miniaudio 回退分别运行 |
| `control_contract` | 控件树生命周期、状态、键盘/鼠标传播、label/button/fps 绘制、PushTarget 与 sys_edit 平台契约 |
| `array_contract` | 内部动态数组的复制、赋值、缩容、清空后再增长及所有权释放 |
| `camera_frame_copy` | 不依赖设备的 BGRA stride/长度/溢出预检、逐行复制与目标缓冲 guard（所有平台） |
| `camera_device_lifecycle` | 不依赖视频 fixture 或 GUI 的 provider 创建、设备枚举、失败打开、状态与重复关闭；Linux CI 会实际加载 V4L2 provider |
| `camera_capture` | 视频 fixture 驱动的相机 open/start/grab/image-copy/stop/close 生命周期（macOS/Windows） |
| `camera_capture_headless` | 同一 fixture 的无窗口 provider 状态机与原始帧测试，用于无 GPU 的 hosted runner 和 sanitizer |
| `input_backend` | 键盘、鼠标、Escape、Command+Q 和事件队列语义（OpenGL 后端） |
| `page_backend` | 离屏页、active/visual page 和页面复制（OpenGL 后端） |
| `window_backend` | 标题、尺寸、位置、关闭状态、`delay_ms` 事件泵与窗口事件（OpenGL 后端） |
| `gdi_lifecycle` | Windows 默认 GDI 的 HWND、BitBlt 呈现、Escape、`delay_ms`、WM_CLOSE 与重用 |
| `gdi_process_exit` | Windows demo 从 `main` 正常返回时，UI 消息线程能在超时前退出 |
| `opengl_process_exit` | OpenGL 默认关闭强制退出及 `INIT_NOFORCEEXIT` 可重用语义 |
| `attach_hwnd` | Windows 真实宿主消息线程、子窗口关联和关闭生命周期；OpenGL 构建运行 GDI/OpenGL 两种变体 |
| `console_contract` | 以隔离的无控制台子进程验证控制台分配、清理、显示/隐藏、键盘查询/读取和关闭，不影响调用测试的终端 |
| `inputbox_contract` | 以隔离子进程和原生 EDIT 控件注入验证 ASCII/Unicode 模态输入、返回值与退出；OpenGL 构建运行 GDI/OpenGL 两种变体 |
| `performance_diagnostics` | Debug 下验证重复 CPU Bitmap 整图上传和像素缓冲访问触发的真实 GPU 回读诊断、进程内去重、`updatebuffer`/缓存读取无误报、GDI/运行时关闭无输出及重定向日志无颜色；可见 popup smoke 仅显式运行 |
| `putimage_basic` | 基础图片复制和缩放 |
| `putimage_alphablend` | alpha blend |
| `putimage_transparent` | 透明色复制 |
| `putimage_rotate` | 图片旋转和缩放旋转 |
| `putimage_comparison` | 多种图片路径的结果对比 |
| `putimage_alphablend_comprehensive` | alpha 边界值和组合场景 |
| `putimage_performance` | 多分辨率图片操作性能基准 |
| `image_buffer_performance` | `getbuffer` 首次/缓存读回、可写访问转换、保留 CPU Bitmap 指针后的逐次上传，以及独立 GPU→GPU `getimage` 复制性能基准 |
| `pixel_access_performance` | 完整像素 get/put 家族、GPU 写后读同步、三种 `getbuffer` 访问意图、持久 CPU Bitmap 读写与逐次上屏，以及 `updatebuffer` 的 1 像素、64×64、全帧双后端性能基准；每项同时验证结果像素和存储模式 |

`default_build_contract` 在 Windows 优先复用父构建已验证的 MSVC，并以
`Ninja Multi-Config` 做空目录配置探测，避免非交互 CTest 中嵌套 MSBuild 的进程跟踪
干扰；探测命令不会传入任何后端选项，因此仍会验证 Windows 默认 GDI 的真实契约。

### 图片传输正确性矩阵

`putimage_*` 专项程序是计时/压力程序，不包含像素断言；贴图功能门禁集中在
`rendering_correctness`，并在 Windows GDI 与 Windows OpenGL 模式下运行同一组断言。

像素缓冲的兼容与同步契约见 [`../doc/pixel-buffer-sync.md`](../doc/pixel-buffer-sync.md)，慢路径
诊断的触发条件和运行时开关见
[`../doc/performance-diagnostics.md`](../doc/performance-diagnostics.md)。

| 接口族 | 确定性断言 |
| --- | --- |
| `getbuffer` | const/访问意图/兼容可写重载、非法参数、存储类型查询与显式转换、IMAGE/当前绘图目标、GPU→CPU 首次回读、持久 CPU Bitmap 每次上屏上传、保留指针跨 EGE 绘制继续写入、普通绘图状态/GDI+ 仿射变换与画刷/resize 迁移，以及基础、拉伸、透明、Alpha、旋转、增强绘图和 `getimage` 的 CPU→GPU 采样桥与反向 GPU→CPU Bitmap 混合路径 |
| `getHDC` | Windows GDI IMAGE 直接访问；OpenGL IMAGE 保留像素和状态地提升为 CPU Bitmap，并验证原生 HDC 写入成为权威像素 |
| `updatebuffer` | 紧密和带 padding 的自上而下源行、IMAGE/当前屏幕目标、物理坐标不受 viewport 影响、GPU 存储保持、与后续 GPU/CPU 操作排序，以及空指针、越界、空区域和非法 stride |
| `getimage`、`putimage` | 屏幕/IMAGE、整图/源矩形/拉伸重载，源和目标 viewport，裁剪、GPU/CPU 缓冲同步、自身重叠复制 |
| BitBlt ROP | 15 个标准三元光栅操作（包括 pattern、blackness、whiteness）逐像素验证 |
| `putimage_transparent`、`putimage_alphatransparent` | 默认范围、显式源矩形、负目标裁剪、颜色键、全局 alpha、目标 alpha 保留 |
| `putimage_alphablend` | 4 个重载，RGB32/ARGB32/PRGB32，0/128/255 alpha，源矩形、viewport、拉伸、平滑采样及 GDI/OpenGL 混合目标 |
| `putimage_withalpha`、`putimage_alphafilter` | 两个 with-alpha 重载、PRGB 合成、平滑拉伸、mask stride/零值/null、GPU 目标同步及 GDI/OpenGL 双向混合 |
| 旋转接口 | `putimage_rotate`、`putimage_rotatezoom`、两个 `putimage_rotatetransparent` 重载，中心坐标、非方形旋转、缩放、全局 alpha、零值透明和平滑路径 |
| 增强贴图 | `ege_gentexture` 开/关、3 个 `ege_puttexture` 重载、2 个 `ege_drawimage` 重载、纹理填充、变换、生成后的源更新及 resize 生命周期 |
| 图片格式 | PNG/BMP 普通及 alpha 往返、JPEG/GIF/TGA/PPM fixture 解码、尺寸/方向/像素/文件头，`saveimage` 分派、`getimage_pngfile`，以及 Windows EXE 内嵌 PNG 的 char/wchar 资源重载 |
| 像素格式 | `image_convertcolor` 的 ARGB/PRGB 双向转换与舍入 |

兼容性说明：文件和资源型 `getimage` 的 `zoomWidth`/`zoomHeight` 参数在既有 Windows
实现中未参与解码或缩放，本轮保持该行为，不把它描述成已生效的缩放功能。新增贴图重载或
后端分支时，应先更新上表并补充同一组 GDI/OpenGL 像素断言。

## 公共 API 覆盖审计

从仓库根目录运行公共声明静态审计：

```bash
python tests/tools/audit_public_api_coverage.py --summary
```

审计同时检查 `ege.h` 与 `ege.zh_CN.h` 的导出函数名、标准化声明、返回类型和参数类型
是否一致，并扫描测试和 demo 中的真实调用（忽略注释与字符串）。当前 291 个公共函数名
均有直接自动化测试调用；去重后的 386 个公共声明都有参数个数证据。
`public_api_overload_test.cpp` 还会让编译器选择每一个精确函数指针类型，审计脚本再校验
这份编译证据与两个公共头一致。新增声明若没有调用或精确重载证据，审计会返回失败。

这是声明与入口覆盖，不等价于每个默认参数、错误分支、硬件设备和平台路径都已覆盖。
重要的 char/wchar、IMAGE/当前目标及后端重载仍需显式行为断言；双后端共享行为必须由
相同断言分别运行，窗口/输入生命周期及模态交互接口使用带超时保护的后端测试，真实
相机、音频设备和 demo 视觉验证作为更高层的补充。

## 构建与运行

从仓库根目录配置原生 Debug 构建：

```bash
cmake -S . -B build/native-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEGE_BUILD_OPENGL=ON \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_ENABLE_CAMERA_CAPTURE=ON
cmake --build build/native-debug
cmake --build build/native-debug --target demos
ctest --test-dir build/native-debug --output-on-failure -LE performance
```

Linux/macOS 的原生可执行文件没有 `.exe` 后缀。Windows 多配置生成器可在构建和测试命令中增加 `--config Debug` 或 `-C Debug`。

Windows 下从 Git Bash（或用 `bash -l` 调用 Git Bash）分别构建并运行 GDI/OpenGL
功能回归。脚本会使用互不复用的 `build/gdi` 与 `build/opengl`：

```bash
bash -l tasks.sh --gdi --debug --load --build -- \
  -DEGE_BUILD_TEST=ON -DEGE_BUILD_DEMO=ON
bash -l -c 'ctest --test-dir "build/gdi" -C Debug --output-on-failure -LE performance'

bash -l tasks.sh --opengl --debug --load --build -- \
  -DEGE_BUILD_TEST=ON -DEGE_BUILD_DEMO=ON
bash -l -c 'ctest --test-dir "build/opengl" -C Debug --output-on-failure -LE performance'
```

Windows OpenGL 构建会保留上述默认 GDI 用例，并额外注册带 `_opengl` 后缀的
`rendering_correctness` 和 `putimage*` 用例。这些变体通过 `INIT_OPENGL` 启动，
使用与 GDI 基准完全相同的像素与性能断言；`input_backend`、`page_backend`、
`window_backend` 和 `opengl_process_exit` 则覆盖 OpenGL 专用窗口路径。
GitHub 托管 Windows runner 负责 OpenGL 编译和默认 GDI 运行门禁；需要 WGL 的完整
运行时回归由可手动触发的 `windows-opengl-runtime.yml` 在带 `opengl` 标签的
self-hosted Windows runner 上执行。

Release demo 截图验证应使用独立目录，并显式关闭开屏动画、强制运行时后端：

```bash
bash -l tasks.sh --gdi --release --build-dir build/demo-visual-gdi \
  --target demos --load --build -- \
  -DEGE_BUILD_TEST=OFF -DEGE_BUILD_TEMP=OFF \
  -DEGE_DEMO_VALIDATION_BACKEND=GDI

bash -l tasks.sh --opengl --release --build-dir build/demo-visual-opengl \
  --target demos --load --build -- \
  -DEGE_BUILD_TEST=OFF -DEGE_BUILD_TEMP=OFF \
  -DEGE_DEMO_VALIDATION_BACKEND=OPENGL
```

`EGE_DEMO_VALIDATION_BACKEND` 只影响 demo 验证目标：它移除 `INIT_WITHLOGO`，并强制
选择 GDI 或 OpenGL，不改变库的默认行为。截图抽样至少覆盖基础图元、文字/CJK、
alpha、旋转透明、viewport、分页以及一个复杂动画；动画应固定同一帧或同一状态再比较。

三个会自行保存固定第 10 帧并退出的 demo 可直接做可重复像素对比（需要 Pillow）：

```bash
python tests/tools/compare_demo_screenshots.py \
  --gdi-build build/demo-visual-gdi \
  --opengl-build build/demo-visual-opengl
```

脚本检查尺寸、平均绝对误差和大差异像素比例，并在
`build/visual-results/current` 输出两后端截图、增强差异图与 JSON 报告。其余交互式 demo
仍通过窗口自动化做启动和画面抽查；固定帧比较不应被视作所有交互状态的替代品。

只运行功能回归、跳过耗时性能基准：

```bash
ctest --test-dir build/native-debug \
  --output-on-failure \
  -LE performance
```

运行全部性能程序（包含多个高分辨率压力基准）：

```bash
ctest --test-dir build/native-debug --output-on-failure -L performance
```

Windows Release 下可用同一工具交替启动 GDI/OpenGL 独立进程，避免固定启动顺序造成
热状态偏差，并生成逐轮原始数据和汇总对比：

```powershell
powershell -NoProfile -File tests/tools/run_pixel_performance_comparison.ps1 `
  -Runs 5 `
  -Configuration Release `
  -GdiBuildDirectory build/gdi `
  -OpenGlBuildDirectory build/opengl
```

`pixel_access_performance` 对每项执行 3 次预热和 21 次计时，报告中位数、P95、均值、
标准差和每操作耗时；对比工具再取 5 个独立进程中位数的中位数。输出目录包含每轮日志、
`raw.csv`、`comparison.csv` 和 `environment.json`。性能结果不设置跨机器硬阈值，像素错误或
存储模式错误仍会使测试失败。

只运行渲染正确性测试：

```bash
ctest --test-dir build/native-debug \
  --output-on-failure \
  -R '^rendering_correctness$'
```

无桌面的 Linux 环境需要虚拟显示：

```bash
xvfb-run -a ctest --test-dir build/native-debug --output-on-failure -LE performance
```

## Sanitizer 回归

Clang/GCC 可用 AddressSanitizer 和 UndefinedBehaviorSanitizer 验证功能测试：

```bash
cmake -S . -B build/native-sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEGE_BUILD_OPENGL=ON \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=OFF \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/native-sanitize
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir build/native-sanitize \
  --output-on-failure \
  -LE performance
```

`camera_frame_copy` 在关闭相机模块时仍会运行，因此 Linux sanitizer 能覆盖帧布局算法。启用相机模块时，`camera_device_lifecycle` 会在没有物理设备的 hosted runner 上验证 provider 的枚举、失败与清理路径；发现设备时还会尝试无自动启动地打开并关闭。macOS 的相机集成 sanitizer 需启用 camera，并同时给 `CMAKE_CXX_FLAGS` 与 `CMAKE_OBJCXX_FLAGS` 增加相同参数。

`detect_leaks=0` 表示该命令不提供泄漏门禁；越界访问和未定义行为检查仍然启用。macOS 的 AddressSanitizer 不支持 LeakSanitizer，因此 CI 明确关闭该选项。

## TDD 约定

1. 先在最小尺寸的 `IMAGE` 上写出会失败的像素或状态断言。
2. 用 Windows 旧后端行为、现有 demo 或明确的 API 文档作为预期基线。
3. 修复共享 API/`RenderTarget` 实现，避免只在测试中绕过真实入口。
4. 先运行目标测试，再运行全部功能测试，最后运行 Release 全量测试和 demo 构建。
5. 涉及文件格式时，必须保存到临时文件后重新加载，验证尺寸、方向、颜色和 alpha，而不只检查返回码。
6. 修改平台默认值时，必须从空构建目录配置且不传被测选项；不得用 CI 显式参数掩盖默认配置错误。

新增测试源文件后，在 `tests/CMakeLists.txt` 中通过 `add_test_executable` 和 `add_test` 注册；窗口测试应能在 Xvfb 下自动完成，不等待人工输入。
