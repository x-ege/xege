# 跨平台重构分支合入与影响分析报告

- 更新日期：2026-07-19
- 分支：`feature/opengl-backend`
- 对比基线：`origin/master@e85aa27`
- 报告范围：以本报告所在提交为准；最终 SHA 与最新 checks 以 PR 页面为准
- 子模块：`3rdparty/ccap@d1876005be7e7cc0c370fadd05dbac6c658c4a17`

## 执行结论

当前结论是 **Conditional Go（代码层面具备合入条件，流程与硬件验收仍有前置项）**。

所有已知的确定性代码问题都已完成修复。此前候选已取得完整的 Native、MSVC、MinGW、交叉编译和 release package 远端证据；后续默认配置、Linux GLFW 和 camera provider 测试改进还必须由本 PR 的最新 checks 重新确认。没有发现 Windows 公共 API/source breaking change，Windows 默认构建仍走 GDI，OpenGL 仍是显式 opt-in。

不建议绕过 PR 直接合入 master。合入前仍应完成三件事：

1. 由维护者审查手写后端与兼容层，并把生成的 GLAD/STB 文件与手写代码分开看。
2. 在真实 Windows 设备上做相机与窗口退出 smoke test；在物理 macOS 上人工确认 Escape、Command+Q 和窗口关闭。
3. 给 master 配置 required checks，并以 PR 最新提交的完整 checks 作为合入依据。

| 维度 | 证据 | 判定 |
| --- | --- | --- |
| 基础绘图、图片与保存 | 像素、状态、文件格式和往返测试；本地与 Linux/macOS CI 全绿 | Go |
| 相机状态机与帧安全 | fixture、headless provider、stride/长度 guard、ASan/UBSan、ccap regression | Go |
| 事件循环与退出 | Escape、Command+Q、WM_CLOSE、默认进程退出、NOFORCE 重用测试 | Go |
| Windows 默认兼容 | MSVC/MinGW GDI、raw static-library consumer、x86/x64 release package 全绿 | Go |
| Windows/macOS 真实硬件 | hosted runner 无摄像头，Windows 无可靠 WGL，系统级按键注入未作为证据 | 人工前置项 |
| PR 与分支治理 | PR 最新提交仍需完整 checks；master required checks 需由维护者确认 | 流程前置项 |

## 本轮完成的修复

### 绘图、图像与保存工具

- 为基础图元、像素、线型、填充、viewport、page、文字、变换、`putimage*`、alpha/transparent/rotate/blur 建立确定性的像素和状态断言。
- `saveimage`、`savepng`、`savebmp` 覆盖 PNG、BMP、大小写混合扩展名、无扩展名默认 PNG、宽字符非 ASCII 路径、空路径和非法目录。
- 保存结果不仅由 EGE 重新加载，还独立校验 PNG signature/IHDR 与 BMP file/DIB header、尺寸、位深、压缩和 channel mask。
- Unix 宽字符路径改为稳定的 UTF-8 转换，避免 locale-dependent `wcstombs` 导致保存失败。
- 修复 OpenGL image 的 `setbkcolor` 像素替换、Windows alpha 路径 viewport 二次平移，以及编译了 OpenGL 但实际运行 GDI 时错误调用 `glReadPixels` 的问题。
- 恢复 blur 核、强度与 alpha 的旧版像素语义，防止跨平台重构悄然改变公开行为。

### 相机

- 修复 `CameraCapture::open(..., false)` 丢失 `autoStart=false` 的状态机错误，并允许 `close()` 后重新 `open()`。
- ASan 复现并修复 provider buffer 比目标 `IMAGE` 多 64 bytes 时的 heap-buffer-overflow。
- 帧复制现在先验证宽高、packed byte count、stride、源长度、目标长度和整数边界，再逐行复制有效 BGRA 像素；分辨率变化会重建缓存图像。
- `camera_frame_copy` 在 camera-off 构建中也执行，覆盖 padded/tight stride、provider trailing bytes、截断尾行、短 stride、null、短 destination 和 guard 区。
- fixture 覆盖 open/start/grab/getImage/copyImage/stop/restart/close/reopen；camera demo 启动 smoke 能完成设备枚举并收到新帧。

### 事件循环与退出

- OpenGL 后端加入与旧 Win32 语义对齐的默认强制退出和 `INIT_NOFORCEEXIT` 可重用路径。
- Command+Q 进入统一 close callback；Escape 保留为 EGE key message；`delay_ms` 持续泵送窗口事件。
- 添加独立子进程测试：默认关闭若返回到调用点或进程挂住，CTest 会失败或超时。
- Win32 UI owner thread 负责销毁 HWND、`PostQuitMessage` 和 join，避免程序从 `main` 返回后消息线程阻止退出。
- 修复 `graph_sort_visualization` 收到 Escape 后只刷新界面却不退出循环的问题。
- 修复静态析构顺序：在 OpenGL context 仍存活时清理 image manager，消除 performance 测试通过后在进程退出阶段崩溃的问题。

### Demo 与构建系统

- 37 个核心 demo 在 macOS、Linux 和 Windows 矩阵中构建。
- GMP demo 仍是可选项，但现在只有 `gmpxx.h` 与实际 GMP library 同时可用时才加入；不完整的 runner 环境会正确跳过，而不是在编译/链接中途失败。
- macOS Homebrew 交叉编译移除无意义的 `brew update` 和重复 CMake 安装，关闭自动更新，并只对 `mingw-w64` 做三次短重试。
- 新增空构建目录的默认配置契约：Windows 必须默认 GDI，Linux/macOS 必须默认 OpenGL，单配置生成器必须默认 Release。
- Linux bundled GLFW 与文档保持一致，默认 X11、显式 opt-in Wayland；调用者显式设置的 GLFW 选项不会被覆盖。
- Native workflow 新增 Wayland-only 编译任务，并让 Linux 主构建直接使用项目的 X11 默认值，不再用 CI 参数掩盖默认配置。
- 新增无 GUI、无 fixture 的 camera device lifecycle 测试；Linux CI 会实际加载 V4L2 provider 并验证枚举、失败打开和清理路径。

## 测试证据

### 当前 macOS 本地

- 37/37 核心 demo 编译、链接成功。
- 功能测试：11/11 passed，7.85s；新增默认配置契约和 camera device lifecycle 均通过。
- canonical performance：1/1 passed，7.04s，并正常完成进程析构。
- Debug、Release 与 camera-on ASan+UBSan 功能套件均为 11/11。
- ASan+UBSan canonical performance 为 1/1，覆盖此前的进程退出阶段崩溃。
- demo startup smoke：37/37 在观察窗口内无启动崩溃；两个 camera demo 均完成设备枚举并产生新帧。
- macOS 系统级按键注入没有可靠送达目标进程，因此没有把该操作包装成人工退出证据；退出结论来自确定性的 callback、状态与子进程测试。

### 此前候选的 GitHub Actions 基线

以下记录证明重构基线曾完整通过；它们不能替代本报告所在提交在 PR 上触发的最新 checks。

| Workflow | 结果 | 覆盖 |
| --- | --- | --- |
| [Native OpenGL Build and Test](https://github.com/x-ege/xege/actions/runs/29657327825) | 10/10 jobs success | Linux Debug/Release/system GLFW/ASan+UBSan；macOS ARM64 Debug/Release、Intel Release、camera sanitizer；Windows OpenGL opt-in build；ccap |
| [MSVC GDI Build and Test](https://github.com/x-ege/xege/actions/runs/29657327822) | 4/4 jobs success | v143/v142 × Debug/Release；GDI functional、退出、相机 fixture、wide-path save、raw `graphics.lib` consumer |
| [MinGW Windows Build](https://github.com/x-ege/xege/actions/runs/29657327820) | 6/6 jobs success | MSYS2、Code::Blocks GCC 14.2、CLion GCC 13.1 × Debug/Release；核心 demos；MSYS2 GDI CTest 与 raw consumer |
| [MinGW Cross-Compile Build](https://github.com/x-ege/xege/actions/runs/29657327829) | 4/4 jobs success | Ubuntu/macOS × Debug/Release；Windows target 与 PE/COFF archive 断言；核心 demos |
| [Release Package dry-run](https://github.com/x-ege/xege/actions/runs/29657332498) | 19 jobs success，发布 job 按设计 skipped | v141/v142/v143 × x86/x64 × Debug/Release；3 种 Windows MinGW；2 种 cross；组包、归档和 assembled consumer |

release dry-run 使用 `workflow_dispatch`，不会创建 GitHub Release。最终组装包实际用包内 public headers 和 library layout 编译了 `camera_base.exe` 与 `graph_5star.exe`，并验证为 PE/COFF。

最终 ccap job 执行 1,016 个设备无关选择用例：1,002 passed、14 capability-dependent skipped、0 failed。它覆盖 conversion/helper、CLI、file playback、memory/backpressure 和 writer，但不等于真实摄像头验证。

## GitHub CI 是否足够

结论：**对本次跨平台重构的高风险路径已经足够作为合入门禁，但不是全 API、全硬件覆盖。**

当前 CI 能阻断以下回归：

- Windows/macOS/Linux 的默认后端、bundled GLFW provider 和单配置构建类型漂移；
- OpenGL/GDI 基础渲染、图片操作、保存格式和公开状态语义；
- camera bridge 的越界、stride、状态机和 fixture 生命周期；
- Linux V4L2 provider 的创建、枚举、失败打开和幂等清理；
- Escape、Command+Q、WM_CLOSE、默认退出、NOFORCE 重用与 teardown；
- Windows 默认 GDI、OpenGL opt-in 编译、MSVC/MinGW/交叉编译；
- bundled GLFW 的 Linux X11 默认构建，以及显式 Wayland-only 编译；
- 旧 VS toolset、x86 frame layout、预编译包目录与 raw static-library consumer；
- release package 漏库、错误 archive、错误目标平台和不可消费的发布包。

不能声称所有接口都有独立单元测试。去除注释后的静态启发式统计中，`ege.h` 有 288 个不同 EGEAPI 名称，其中 117 个被测试源码直接引用，约 40.6%。场景测试会间接覆盖更多共享入口，但当前没有 line/branch coverage 数据，不能把 40.6% 当作真实代码覆盖率。

仍未自动化覆盖的边界：

- Windows MSMF/DirectShow 的真实设备枚举、权限、无帧回退、分辨率切换与设备切换；
- Linux V4L2 的真实设备首帧、格式协商、分辨率切换与长时间稳定性；
- hosted Windows runner 上可靠的 WGL runtime；当前显式 OpenGL tests 会编译，运行门禁使用确定性的 GDI compatibility tests；
- hosted macOS 的完整 accelerated NSGL 渲染；像素与窗口 runtime 由 Linux Xvfb 和物理 Mac 补位；
- OS 真实菜单触发的 Command+Q 与真实键盘 Escape；
- ccap 的真实 camera CLI、硬件 performance、x86 AVX2 和三个 orientation harness 误报用例；
- 每个长尾 EGE API 的独立行为测试。

## Windows breaking-change 与影响

### 已确认保持的兼容面

- Windows 顶层 CMake 默认仍是 `EGE_BUILD_OPENGL=OFF`；旧项目不会被静默切换到新后端。
- `INIT_OPENGL=0x80` 仅在 OpenGL build 宏存在时可见，不改变旧 `initmode_flag` 的数值。
- 当前分支与 `origin/master` 的 288 个 EGEAPI 名称集合完全相同，没有公开函数删除或重命名。
- `graphics.lib`、`graphicsd.lib`、`libgraphics.a` 名称，以及 VS2017/VS2019/VS2022、x86/x64、Debug/Release 目录保持。
- `IMAGE` 在 public header 中仍是不透明前置声明；新增 RenderTarget/Window 字段不暴露为 public class layout。
- 默认 GDI 继续使用旧 HWND、UI message thread 和 BitBlt；OpenGL build 的 `getHWnd()` 在 Windows 返回真实 GLFW HWND。
- 相机所需 Media Foundation/Shlwapi/Propsys 依赖由构建目标、pragma 与 package consumer smoke 承接。

### 合入后的实际影响

- **默认 Windows GDI 用户**：预期不需要修改源码或构建参数；远端 v142/v143 和 MinGW GDI 行为测试均已通过。
- **旧预编译包用户**：目录和库名保持，v141/v142/v143 的 x86/x64 Debug/Release 全部重新构建并完成 consumer 验证。
- **Windows OpenGL 用户**：获得显式 opt-in 新能力；首版仍应标记为 experimental，不能把 hosted runner 的编译证据等同于真实 GPU/WGL 兼容性。
- **相机用户**：获得状态机和越界修复；真实 Windows 设备兼容仍需硬件 smoke。

综合判断：**未发现 Windows source breaking change，默认路径的侵入性已经较低。** 但共享 image/font/input/teardown 代码改动较大，不能把“没有检测到 breaking change”写成绝对 ABI/行为保证。

## 其他平台影响

- macOS/Linux 顶层源码构建现在默认走 native OpenGL，不再默认产出供 Wine 使用的 Windows binary；这是本分支最大的有意构建语义变化，必须写入 release notes。
- Linux 增加 bundled/system GLFW 两条路径、Xvfb 功能测试和 sanitizer。
- macOS 增加 ARM64/Intel 编译、物理机 runtime 路径和 camera sanitizer；hosted runner 的 NSGL 限制仍存在。
- CI 和 release 时间明显增长，这是维持旧 Windows 包、跨编译器和 raw consumer 兼容的成本。

## 范围与清理审计

- 提交前必须执行 `git diff --check`，并只显式暂存本轮跨平台构建、测试、CI 和文档文件。
- `3rdparty/ccap` 固定在已验证 SHA；没有把临时 vendor 副本作为普通源码提交。
- 无关的未跟踪 `3rdparty/libpng/` 与 `3rdparty/zlib/` 已移出工作树，未进入 Git 历史。
- build 产物、测试 PNG/BMP 和下载压缩包均未进入提交。
- 轻量 secret-pattern 扫描无匹配；本机未安装 gitleaks，因此这不是完整凭据扫描。
- 约 27K 行新增来自生成的 GLAD/STB；PR review 应把生成文件与手写 backend/compatibility diff 分开。

## 合入前清单

本地自动化条件已经满足；远端条件以 PR 最新 checks 全绿为准。真正 merge 前建议只保留以下阻断项：

1. 复核 PR 最终 diff、submodule SHA 和本报告所列 workflow，并确认最新提交全绿。
2. master required checks 至少包含 Native Linux Release、Linux sanitizer、ccap、MSVC v143 Release、MSYS2 Release、Ubuntu cross Release 和 release package。
3. 真实 Windows：相机枚举/首帧/切换/关闭，Escape、窗口关闭、进程退出；真实 macOS：Escape、Command+Q、窗口关闭。
4. 对 ccap orientation harness 的三个临时排除项建立 owner 和后续修复记录，避免永久静默过滤。
5. 在 release notes 明确 macOS/Linux 默认构建语义变化，以及 Windows OpenGL 仍为 opt-in/experimental。

非阻断后续项：逐 API family 增加测试和覆盖率统计、为固定 WinLibs 下载增加 SHA-256、升级产生 Node runtime warning 的 GitHub Actions、扩展 Linux V4L2/Windows MSMF 真机测试，以及在 headless Wayland compositor 上增加 runtime smoke。

最终建议：以 PR 最新全绿提交作为代码候选。若上述人工硬件与分支治理项完成，可以合入；若团队选择把真实 Windows/Linux 相机验证延后，必须在 PR 中显式记录风险与 owner，而不是把它当作 CI 已覆盖。
