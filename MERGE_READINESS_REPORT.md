# 跨平台重构分支合入与影响分析报告

- 更新日期：2026-07-19
- 本地证据快照：2026-07-18
- 分支：`feature/opengl-backend`
- 对比基线：`origin/master@e85aa27`
- 审计起点 HEAD：`7a63b58`

## 执行结论

当前判定是 **No-Go，暂不建议直接合入 master**。

这里的 No-Go 已不是“本地仍有已知代码失败”：本轮发现的绘图、图片保存、相机帧越界、相机状态机、窗口关闭和事件循环问题均已完成 TDD 修复，本地 Release、Debug 和 sanitizer 门禁均为绿色；5 个 workflow 的静态审计也没有剩余 blocker。

真正缺少的是合入所需的远端证据：截至审计快照，本分支没有 PR 或 GitHub Actions 运行；真实 Windows、Linux、MinGW cross 和 release package 流程尚未执行；master 目前没有 branch protection。本轮候选改动可以提交并推送以触发最终矩阵，但在矩阵取得结果前不适合直接合入。

| 判定维度 | 当前状态 | 结论 |
| --- | --- | --- |
| macOS 本地实现与 TDD | Release/Debug/ASan+UBSan 全绿 | Go |
| workflow 静态结构 | YAML、Shell、权限、矩阵表达式审计通过 | Go |
| Linux runner | 仅有 workflow，尚无本分支运行 | 待验证 |
| Windows GDI/OpenGL | 仅有 workflow 和静态分析，尚无 runner 结果 | 待验证 |
| MinGW cross/release package | 目标断言和 consumer smoke 已补，尚未实跑 | 待验证 |
| 真实 Windows 相机与交互退出 | hosted runner 无法覆盖 | 待人工验证 |
| 提交/PR/required checks | 候选改动随本报告提交；PR 与 required checks 仍未完成 | No-Go |

## 分支和提交范围

- 审计起点相对 `origin/master`：0 behind / 23 ahead；本轮候选提交会在此基础上形成新的分支 HEAD。
- 本轮候选提交纳入 46 个已修改文件，以及 16 个新增源码、测试、workflow 和报告文件；无关的 `3rdparty/libpng/` 与 `3rdparty/zlib/` 临时源码树已移出工作树，不进入提交。
- 审计时 `git diff --stat origin/master` 的 tracked 快照为 84 个文件、37,084 行新增、1,099 行删除；最大部分是 vendored GLAD/STB 与 OpenGL 后端，整体仍属于高审查成本重构。最终范围应以候选提交的 PR diff 为准。
- `git diff --check origin/master` 当前为 clean。
- GitHub 上没有本分支 PR，`gh run list --branch feature/opengl-backend` 没有运行记录；master branch protection API 返回未保护。

## 本轮完成的关键修复

### 绘图和图像工具

- 补齐基础图元、线型、填充、viewport、page、图片混合/透明/旋转、文字及状态行为的像素和场景测试。
- 恢复 `imagefilter_blurring_4/8` 与旧版本一致的核、强度和 alpha 行为，避免公开像素语义变化。
- `saveimage`/`savepng`/`savebmp` 覆盖 PNG、大小写混合 BMP、无扩展名默认 PNG、宽字符非 ASCII 路径、空路径和非法目录。
- 保存结果除 EGE 重新加载外，还独立解析 PNG signature/IHDR 与 BMP file/DIB header、尺寸、位深、压缩和 channel masks。
- 修复 Unix 宽字符路径使用 locale-dependent `wcstombs` 导致保存失败的问题，统一使用 UTF-8 转换。

### 相机

- 修复 `CameraCapture::open(-1, false)` 丢失 `autoStart=false` 的状态机错误。
- camera fixture 覆盖 open/start/grab/getImage/copyImage/stop/restart/close/reopen。
- ASan 实际复现：320×240 `IMAGE` 为 307,200 bytes，而 ccap provider 缓冲区为 307,264 bytes；旧代码按 `sizeInBytes` 整块复制，产生 64-byte heap-buffer-overflow。
- 现在先验证宽高、packed byte count、stride、源缓冲长度和目标长度，再分配 `IMAGE`；只逐行复制有效 BGRA 像素，并在分辨率变化时重建缓存图像。
- 新增平台无关 `camera_frame_copy` 测试，确定性覆盖 padded stride、tight stride + provider trailing bytes、截断尾行、短 stride、null source、短 destination、整数边界和 destination guard；即使关闭 camera 模块也会执行，因此 Linux sanitizer 能覆盖核心算法。
- 新 helper 通过 `-std=c++98 -pedantic-errors` 单独编译，避免给 camera-off 的旧工具链新增 C++11 要求。

### 事件循环和退出

- OpenGL 默认窗口关闭现在与 Win32 兼容：默认模式结束进程，`INIT_NOFORCEEXIT` 保持可关闭后重用。
- 新增独立进程测试，若默认 Command+Q/close 错误返回、或退出路径挂住，CTest 会失败/超时。
- Win32 默认 GDI 不再被未绑定 HWND 的抽象 `Window` 错误接管；仍使用旧 HWND、UI message thread 和 BitBlt。
- Win32 进程析构通过 UI owner thread 销毁窗口、`PostQuitMessage` 后 join，不改变公开 `closegraph()` 的“隐藏并可重用”语义。
- 静态析构在 OpenGL context 存活时释放页面、timer image、输入队列和 Window，避免 teardown 顺序错误。
- 修复 `graph_sort_visualization` 的 Escape 分支只更新 UI、不退出循环的问题。

## GitHub CI 审计与补充

### 修改前的主要缺口

- native OpenGL workflow 尚未进入版本控制，也没有远端运行。
- Windows workflow 多数只编译，不执行 CTest；demo target 和 raw prebuilt consumer 覆盖不完整。
- Windows OpenGL 只有 compile-only，不能证明 GDI 默认与 OpenGL opt-in 共存。
- sanitizer 没有覆盖相机 bridge；ccap 子模块的 1,000+ 测试没有稳定门禁。
- MinGW cross 可能把宿主 native archive 当 Windows 库打包，原 cache 断言也检查了错误文件。
- release workflow 缺 PR dry-run、严格 tag/version 校验、完整旧目录兼容和 assembled package consumer。
- MSVC 主 workflow 曾在 PowerShell 中使用 Bash `\` 续行，会确定性失败。
- 两个 MinGW workflow 未显式收紧 token 权限，手动 `build_type` 输入也未真正裁剪矩阵。

### 当前矩阵

| workflow 路径 | 构建/运行范围 | 关键门禁 |
| --- | --- | --- |
| Native OpenGL | Linux Debug/Release；macOS ARM64 Debug/Release；macOS Intel Release | 全部 demos；14 项功能测试；Linux Release performance |
| Linux system GLFW | Ubuntu Release | 非 bundled GLFW + Xvfb 功能测试 |
| Linux sanitizer | Debug，camera off | ASan+UBSan 全功能；平台无关 camera frame copy 仍运行 |
| macOS camera sanitizer | ARM64 Debug，camera on | C++/Objective-C++ ASan+UBSan；synthetic helper + camera fixture |
| Windows OpenGL opt-in | windows-2022 Release | 同一构建运行默认 GDI lifecycle/exit 与显式 `INIT_OPENGL` 测试；全部 demos |
| ccap regression | macOS ARM64 Release | 1016 个选择用例顺序运行，避免设备/并行基础设施误报 |
| MSVC default GDI | v143/v142 × Debug/Release | GDI CTest、全部 demos；v143 Release raw `graphics.lib` camera/drawing consumer |
| MinGW Windows | MSYS2、Code::Blocks WinLibs、CLion/RedPanda WinLibs | demos；MSYS2 CTest/raw consumer；release workflow 对三种工具链均测试 |
| MinGW cross | Ubuntu/macOS × Debug/Release | 强制 GDI/OFF；检查 `CMAKE_SYSTEM_NAME=Windows` 与 PE/COFF archive；显式 demos |
| Release package | v143/v142/v141 × x86/x64 × Debug/Release；三种 Windows MinGW；两种 cross | x64 功能测试、x86 frame-layout test、精确 artifact、整包 camera/drawing consumer |

额外治理：

- 所有 workflow 默认 `permissions: contents: read`；仅 tag-only `publish-release` job 有 `contents: write`。
- release workflow 在相关 PR 上执行完整 dry-run，但不会发布 Release。
- v* tag 格式、header version、tag version 和 commit 可达 master 均严格校验。
- MinGW 手动触发会只跑选择的 Debug 或 Release；push/PR 仍跑两者。
- PowerShell build 续行已改成 backtick，5 个 workflow YAML 解析、46 个 Bash/MSYS2 script block 语法、权限/矩阵结构和 `git diff --check` 均通过。
- `windows-2022` 当前不预装 v141，workflow 通过 Visual Studio Installer 显式安装微软仍提供的 `Microsoft.VisualStudio.Component.VC.v141.x86.x64`；该策略静态合理，但必须远端首跑证明。[Windows runner 镜像清单](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)，[v141 组件说明](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-professional?view=visualstudio)。
- macOS runner 标签使用 GitHub 当前公开的 hosted runner 体系；实际容量和架构仍以 Actions 首跑为准。[GitHub-hosted runners reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)。

### CI 充分性结论

workflow 的静态设计现在足以作为这次重构的 PR 门禁；但“定义了 CI”不等于“CI 已通过”。在没有远端 Actions 结果和 required checks 之前，不能把当前分支判为可合入。

仍需披露的覆盖边界：

- Windows hosted runner 通常没有真实摄像头，无法验证 MSMF/DirectShow 的设备枚举、权限、无帧回退和硬件切换。
- Windows OpenGL 3.3 context、v141 可选组件安装、WinLibs 下载/构建、MSYS2 camera link、PowerShell/cmd 真实语义只能由远端执行证明。
- 固定 WinLibs 下载 URL 尚无 SHA-256 校验，是非阻塞供应链残余风险。
- macOS sanitizer 使用 `detect_leaks=0`，覆盖越界和 UB，但不提供 leak gate；macOS 不支持 LeakSanitizer。
- ccap 的测试基础设施仍需 workaround：作为 XEGE subdir 时错误使用 `CMAKE_SOURCE_DIR`，test-data 向上搜索可能走到 `/`，CLI 共享临时目录会在并行 CTest 中竞争。因此 CI 采用 standalone build、源树下 build dir 和顺序直跑 executable。

## 当前本地测试证据

### XEGE 顶层

- Release 全量测试：15/15 passed（14 项功能 + 1 项性能），41.70s；性能项本身为 5.39s。
- Debug 功能测试：14/14 passed，90.86s。
- ASan + UBSan 功能测试：14/14 passed，222.96s；最终相机 helper + integration 另行重建复验 2/2 passed。
- camera disabled 构建：`camera_frame_copy` 1/1 passed。
- C++98 compatibility compile：`camera_frame_copy.cpp` 使用 `-std=c++98 -pedantic-errors` 通过。
- demos target：37/37 编译、链接成功。
- demo startup smoke：37/37 每个运行 1.5 秒无启动崩溃，并按捕获 PID 终止。
- 两个 camera demo 延长运行均完成设备枚举并记录 `ege: new frame created!!`。
- 最终自动化输入注入未能让 macOS System Events 把 Escape 送进目标进程，因此没有把该次人工注入当作退出证据；退出结论来自确定性的 key mapping、window close、default process exit 与 NOFORCE reuse 测试。

### ccap 子模块（独立记账）

锚定 submodule `d1876005be7e`，macOS ARM64 Release，file playback/writer 开启：

- GoogleTest/CTest discovery：1054 cases。
- PR blocking 选择：1016 cases = 920 core + 22 filtered CLI + 46 file playback + 6 memory/backpressure + 22 video writer。
- 本机结果：1009 passed / 7 skipped / 0 failed；跨 runner 只应要求 0 failed，pass/skip 数会随相机和 ARM/AVX2 能力变化。
- 38 个未进入阻断集合：8 个硬件敏感 performance、27 个真实摄像头 CLI、3 个 orientation harness false negatives。
- 三个临时排除项为 `FrameOrientationMatchesFFmpeg_BMP/PNG/JPG`：BMP/PNG helper 不接受当前 32-bit 输出，JPG 独立复核 direct SSIM 0.955948、vflip 0.713447，方向实际正确。它们是待修复的测试工具问题，不应永久忽略。
- `ccap_memory_safety_test` 是功能性内存/背压测试，不是 sanitizer 证据。
- 当前 ccap job 仅覆盖 Apple ARM64/AVFoundation Release，不代表 Linux V4L2、Windows MSMF/DirectShow、x86 AVX2、Debug、sanitizer 或真实设备。

## Windows breaking-change 与影响分析

### 明确保持的兼容面

- Windows 顶层 CMake 默认仍是 `EGE_BUILD_OPENGL=OFF`，默认和 release package 继续使用成熟 GDI/GDI+；OpenGL 不会静默接管旧程序。
- `INIT_OPENGL=0x80` 仅在 OpenGL build 宏存在时可见，不移动旧 `initmode_flag` 数值；OFF build 的 configure-time compile gate 会证明该符号不可见。
- 当前与 `origin/master` 的 `ege.h` EGEAPI 函数名集合无新增/删除差异；没有发现公开函数删除或重命名。
- `graphics.lib`、`graphicsd.lib`、`libgraphics.a` 名称和 VS2017/VS2019/VS2022、x86/x64、Debug/Release 目录保持。
- Windows OpenGL 的 `getHWnd()` 返回真实 GLFW HWND；默认 GDI 仍返回原 HWND。
- `IMAGE` 在 public header 中仍是不透明前置声明，新增 RenderTarget/Window 内部字段不构成公开类布局 ABI 变化。
- MSVC 相机对象通过显式 `#pragma comment(lib, ...)` 携带 Media Foundation/Shlwapi/Propsys 依赖；MinGW imported target 和 release package CMake 也显式补齐系统库。
- CameraFrame 的 `shared_ptr` API 变更说明已存在于 `origin/master`，不是本分支新增的 breaking change。

### 合入后 Windows 用户的预期影响

- 默认 GDI 用户：预期无需修改源码或构建参数；窗口创建、BitBlt、输入、`delay_ms` 和 closegraph 走旧后端，但共享 image/font/input/teardown 实现变化仍必须由 Windows CTest 证明。
- 相机用户：获得 file fixture、状态机和帧边界修复；静态库会新增/显式承接 Media Foundation 系统依赖，但 raw consumer 测试用于保证旧的“只链接 graphics library”体验。
- OpenGL 用户：获得显式 opt-in 的新 Windows 路径；这是新增能力，不应在首版承诺与 GDI 完全相同的渲染质量或硬件兼容性。
- 预编译包用户：v141/v142/v143 目录仍在；release dry-run 和 assembled consumer 会阻止漏库或目录漂移。

### 剩余 Windows 风险

- 共享 `resizewindow`、字体、图像、输入队列和析构路径改动较大，本机 macOS 不能验证 Win32 message ordering。
- v141 虽能安装为 VS2022 可选组件，但实际 ABI/SDK/packaging 组合必须由远端四个 v141 matrix 结果证明。
- 27 个真实 camera CLI tests 不在阻断 gate；fixture 和 synthetic frame copy 不能替代真实设备。
- Windows OpenGL hosted runner 可能受远程桌面/驱动影响；应先标记 experimental，直到 runtime job 稳定。

综合判断：没有发现明确的 Windows public API/source breaking change；默认 GDI 的设计侵入性已降到较低，但在真实 Windows CI 和人工硬件验收前，行为兼容风险仍为中等。

## “所有接口是否都有测试”的回答

不能声称每个公开接口都已经有独立单元测试。

去除注释后的静态启发式统计中，`ege.h` 的 288 个不同 EGEAPI 名称有 117 个被测试源码直接引用，约 40.6%。场景测试会间接覆盖更多共享路径，因此该比例不能等同于真实代码覆盖率；但它足以说明 API 长尾仍存在。

本轮优先锁定的是 breaking-risk 最高的行为族：

- 基础图元、像素、线型、填充、viewport、page；
- PNG/BMP 保存和独立格式解析；
- putimage/alpha/transparent/rotate/blur；
- public header 与 OFF/ON compile gate；
- input/window/default exit/NOFORCE reuse/GDI process teardown；
- camera fixture、状态机、padded frame copy 和 sanitizer；
- raw prebuilt consumer、cross target 格式和 release package。

后续应按 API family 增量补齐，而不是机械追求“每函数一个测试”。合入门禁应以兼容风险、可观察行为和平台证据为主。

## 转为 Go 的必要条件

1. 在 PR 中复核完整候选提交范围，确认包含 `native-opengl-build.yml`、相机/窗口/渲染测试与本报告，并确认不包含 `3rdparty/libpng/`、`3rdparty/zlib/`。
2. 创建 PR，并再次确认完整 PR diff 的 `git diff --check`、submodule SHA 和文件范围。
3. Native Linux/macOS、system GLFW、Linux sanitizer、macOS camera sanitizer 和 ccap regression 全绿。
4. MSVC v143/v142 GDI Debug/Release、raw `graphics.lib` consumer 全绿。
5. MSYS2/WinLibs GDI、raw `libgraphics.a` consumer 和 demos 全绿。
6. Ubuntu/macOS MinGW cross 的 `CMAKE_SYSTEM_NAME=Windows` 与 PE/COFF 断言全绿。
7. Windows OpenGL opt-in runtime 全绿；若 hosted runner 不稳定，明确标记 experimental 并拆成非阻断观察任务，但默认 GDI 必须阻断。
8. release workflow PR dry-run 全绿，包括 v141/v142/v143、x86 frame-layout、x64 CTest、精确目录和 assembled camera/drawing consumer。
9. Windows 真实硬件人工验证相机枚举/首设备无帧回退/分辨率切换/关闭，及 Escape、窗口关闭、进程退出。
10. 给 master 配置 required checks，至少包含 native、sanitizer、MSVC v143、MSYS2、Ubuntu cross 和 release dry-run；ccap regression 也应阻断本次相机变更。
11. 三个 ccap orientation harness 在合入前修复，或建立有 owner/期限的临时豁免；不能无记录永久过滤。

## 合入后的整体影响

- macOS/Linux：普通源码构建默认切换到 native OpenGL，不再默认产出依赖 wine 的 Windows binary；这是本分支最大的有意构建语义变化，应在 release notes 显著说明。
- Windows 默认：继续 GDI，预期源码和链接方式不变；新增 CI 会把旧默认路径设为阻断门禁。
- Windows OpenGL：新增 opt-in 能力，首版成熟度应低于默认 GDI。
- 相机：获得 ccap 更新、确定性 fixture、状态机与越界修复；真实设备仍需平台验收。
- 发布维护：release PR 会明显变慢，因为增加 v141/v142/v143、x86/x64、MinGW、cross 和 consumer smoke；代价换来旧包目录和 raw static-library 体验的可验证兼容。
- 仓库维护：新增 GLAD/STB/vendor 代码显著放大 diff；后续 review 应把 vendored 文件与手写 backend/compatibility 代码分开审查。

最终建议：把当前工作整理成一个范围明确的 PR，先让新增矩阵取得第一轮真实证据；只有 Windows、Linux、cross、ccap 和 release dry-run 全绿，且 required checks 生效后，再把结论从 No-Go 调整为 Go。
