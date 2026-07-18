# EGE 跨平台重构 PRD（OpenGL 后端）

## 背景与问题

EGE（Easy Graphics Engine）目前以 Windows 的 GDI/GDI+ 渲染链路为主。项目虽然“名义上跨平台”，但在 Linux/macOS 上通常依赖 **wine + mingw-w64 交叉编译**才能工作，无法称为真正的原生跨平台。

本 PRD 的目标是：在**尽量保持主干行为不变**的前提下，引入 OpenGL 渲染后端，在 Linux/macOS 默认使用 **native 原生构建与运行**，并保留 Windows 的成熟 GDI/GDI+ 路径，从而逐步废弃 wine 那套“伪跨平台”路径。

## 总体目标（Goals）

1. 引入 OpenGL 渲染后端，使 EGE 在 Windows / Linux / macOS 具备跨平台渲染能力。
2. 通过模块化/抽象层拆分窗口与绘制后端，保证未来可扩展更多后端。
3. 以像素级单元测试为行为基线，并用 `demo/` 覆盖真实程序的编译、链接和运行入口。

## 非目标（Non-Goals）

- 不在第一阶段强求 OpenGL 路径完全复刻 GDI/GDI+ 的像素级一致性。
- 不在第一阶段完成所有高级特性（复杂路径、完整文字栅格化、所有混合模式等）。优先保证核心 API 的可用性与稳定性。

## 构建开关：EGE_BUILD_OPENGL（关键约束）

**唯一入口：**在 CMake 中新增/使用 `EGE_BUILD_OPENGL` 选项来控制是否启用 OpenGL 跨平台模式。

**默认策略（对源码仓库/开发者而言）：**

- Linux/macOS 默认 `EGE_BUILD_OPENGL=ON`，直接使用本机编译器和原生 OpenGL 后端。
- Windows 默认 `EGE_BUILD_OPENGL=OFF`，继续使用成熟的 GDI/GDI+ 后端；OpenGL 仍为显式 opt-in。
- Unix 上显式设置 `EGE_BUILD_OPENGL=OFF` 时，才进入历史 mingw-w64 + wine 兼容路径。

> 说明：仓库内可能还存在其它历史/兼容选项（例如 `EGE_BUILD_FOR_LINUX`）。本 PRD 以 `EGE_BUILD_OPENGL` 作为跨平台重构的唯一开关入口，其它选项在开启 OpenGL 时要被约束/收敛为一致行为（见下文）。

### 当 `EGE_BUILD_OPENGL=OFF`

**行为必须与主干分支保持一致**：

1. CMake/代码路径：保持现状，不引入 OpenGL/GLFW 的依赖与构建逻辑。
2. 公共 API：编译期**不提供** `ege::INIT_OPENGL`（即头文件中不应出现该枚举值/符号）。
3. 平台策略：Linux/macOS **继续沿用 mingw-w64 + wine** 的现有模式（保持与主干一致），不承诺 native 构建。

### 当 `EGE_BUILD_OPENGL=ON`

此时项目进入“真正跨平台”模式：

1. **Windows：双后端并存，默认仍走旧后端**
	- 新增 `ege::INIT_OPENGL` 作为 initmode_flag 选项。
	- 只有当用户显式 `setinitmode(... | ege::INIT_OPENGL)` 时，才启用 OpenGL 后端。
	- 若用户不传 `INIT_OPENGL`，则行为保持为现有 GDI 路径（兼容既有用户）。

2. **Linux/macOS：强制使用 OpenGL 后端（不可关闭）**
	- 一旦 `EGE_BUILD_OPENGL=ON`：Linux/macOS 不再使用 mingw-w64，不再依赖 wine。
	- 改为 **native 编译**，并直接依赖系统 OpenGL（以及窗口系统相关依赖）。
	- 在 Linux/macOS 上 `ege::INIT_OPENGL` 视为“始终开启”（无论用户是否设置 initmode），以隔离 Windows-only 的历史包袱并简化平台差异。

3. 代码隔离要求
	- 在 Linux/macOS + `EGE_BUILD_OPENGL=ON` 的组合下：应尽可能隔离/屏蔽所有仅 Windows 可用内容（Win32 API、GDI/GDI+ 专属实现、mingw-w64 特有逻辑）。

## 与现有构建选项的关系（强约束）

### `EGE_BUILD_FOR_LINUX`

- 当 `EGE_BUILD_OPENGL=OFF`：Linux/macOS 进入显式选择的 legacy 兼容路径，cross-compile Windows 目标并在运行时使用 wine。
- 当 `EGE_BUILD_OPENGL=ON`：Linux/macOS 必须使用 native 构建。
	- 这意味着 `EGE_BUILD_FOR_LINUX` 在该组合下应被强制为 `ON`（即使用户手动设置为 OFF 也会被覆盖/报错）。
	- 该组合下应隔离掉 mingw-w64 / wine 相关逻辑；不再生成/运行 Windows 可执行文件。

## API 与行为约束（可测试）

### `ege::INIT_OPENGL` 的编译期可见性

- `EGE_BUILD_OPENGL=OFF`：头文件中 **不出现** `ege::INIT_OPENGL`。
- `EGE_BUILD_OPENGL=ON`：头文件中提供 `ege::INIT_OPENGL`。
  - Windows：只有用户显式传入该标志才切换 OpenGL 后端。
  - Linux/macOS：该标志被视为“强制开启”（忽略用户是否传入），但仍可保留该枚举值以兼容跨平台代码编译。

## 后端技术选型

- 窗口与 OpenGL 上下文：优先使用 **GLFW**。
- OpenGL 函数加载：可使用 **GLAD**（或在构建系统中提供可替代方案）。
- 渲染策略：使用 OpenGL 3.3 Core 上下文与 `RenderTarget` 抽象；绘图语义由可读回像素的离屏页面实现并以单元测试约束。

## 设计与架构（需要明确的抽象边界）

为支持多后端，渲染相关部分需要抽象出：

1. Window 层：负责窗口创建、事件处理、交换缓冲等。
2. RenderTarget 层：负责像素、图元、填充、图片、文字、剪裁和页面呈现。

并提供至少两套实现：

- Windows：GDI（现有）/（可选）OpenGL
- Linux/macOS：OpenGL（强制）

## 兼容性与验收标准

### 编译维度验收（必须）

- `EGE_BUILD_OPENGL=OFF`：Windows/Linux/macOS 的构建行为与主干一致（尤其是 API 暴露与依赖关系）。
- `EGE_BUILD_OPENGL=ON`：
  - Windows：可在不传 `INIT_OPENGL` 的情况下继续使用旧后端构建/运行；传入后走 OpenGL。
  - Linux/macOS：可 native 构建，且能运行至少一个 demo 作为 smoke test。

### 运行维度验收

- 基础图元、线型/填充、图片、文字、视口/变换的结果必须由像素级测试验证。
- PNG/BMP 保存结果必须可重新加载，并验证尺寸、方向、颜色与 alpha。
- 页面、窗口和输入状态必须有不依赖人工操作的回归测试。
- `demo/` 中的全部目标必须在原生构建中完成编译和链接。

## 开发顺序建议（里程碑）

1. CMake 增加 `EGE_BUILD_OPENGL`，并把依赖/编译宏/平台路径分支清晰落地。
2. 在 `EGE_BUILD_OPENGL=OFF` 下确保完全不引入 `ege::INIT_OPENGL` 与 OpenGL 依赖。
3. 在 `EGE_BUILD_OPENGL=ON` 下完成：
	- Windows：`INIT_OPENGL` opt-in。
	- Linux/macOS：native + 强制 OpenGL 后端。
4. 用 `demo/` 做回归，优先保证“能编译”，再逐个提高“能运行”。

## 发布策略（长期演进目标）

为了避免一次性切换导致 Windows 兼容性风险，同时逐步废弃 wine 路径：

1. **Windows 发布包（长期保守策略）**
	- 发布时启用 `EGE_BUILD_OPENGL=ON`。
	- 默认仍走 GDI/GDI+ 旧后端；只有当用户显式传入 `ege::INIT_OPENGL` 才启用 OpenGL 后端。
	- 未来可能将 OpenGL 设为默认，但当前阶段不做该变更。

2. **Linux/macOS 发布包**
	- 默认发布 `EGE_BUILD_OPENGL=ON` 的 native 版本，不依赖 mingw-w64 或 wine。
	- `EGE_BUILD_OPENGL=OFF` 仅作为迁移期的显式 legacy 兼容选项，不再是 Unix 默认路径。

## 现状与进度（截至 2026-07-18）

### 本轮已完成

- **原生构建链路**
	- Linux/macOS 默认启用 OpenGL；原生 Unix 默认构建子模块中固定版本的 GLFW。
	- macOS 摄像头实现改用 Objective-C++ 并链接所需系统 framework。
	- GitHub Actions 覆盖 Linux Debug/Release、macOS ARM64 Debug/Release、macOS Intel Release、Linux system GLFW、Linux sanitizer、macOS camera sanitizer 和 Windows OpenGL opt-in。
	- Windows 默认 GDI 另由 MSVC v143/v142、MSYS2/WinLibs、MinGW cross 和 release package workflow 覆盖；发布矩阵保留 v141 x86/x64 Debug/Release。

- **核心绘图兼容性**
	- 补齐线段、矩形、圆/椭圆、弧、扇形、多边形、圆角矩形、Bezier、像素和 flood fill。
	- 对齐线宽、虚线、端帽、连接、填充轮廓、hatch/user/texture/gradient pattern 及 affine transform 语义。
	- 补齐 viewport 剪裁、离屏页面、页面切换与窗口呈现行为。
	- 补齐图片复制、拉伸、透明色、alpha、旋转/缩放、仿射、模糊和 mask 路径。
	- 补齐跨平台字体发现、UTF-8/宽字符转换、文字度量和文字绘制。
	- 补齐键盘、鼠标、窗口尺寸/位置/标题、关闭状态和事件队列语义。

- **图片工具函数**
	- `saveimage`、`savepng`、`savebmp` 在原生后端可用。
	- PNG 与 BMP 支持普通 RGB 输出和保留 alpha 的输出；alpha BMP 使用 BITMAPV4 布局。
	- 测试覆盖 `.png`、大小写混合 `.BMP`、无扩展名默认 PNG、宽字符非 ASCII 路径、非法路径和空路径。
	- 除重新载入外，还独立解析 PNG signature/IHDR 和 BMP file/DIB header、尺寸、位深、压缩及 channel masks。

- **相机与事件循环**
	- 修复 `open(-1, false)` 丢失 `autoStart`、默认关闭/Command+Q 语义、进程析构 UI thread 卡住及 demo Escape 退出问题。
	- 相机 BGRA 帧先验证尺寸、stride、源长度和 packed byte count，再分配 `IMAGE`；逐行只复制有效像素，不再复制 provider 尾部对齐字节。
	- 新增不依赖设备的合成帧测试，覆盖 padded/tight stride、trailing bytes、截断源、短目标缓冲、溢出与 guard；camera-off/Linux sanitizer 也会执行。

- **TDD 与工程验证**
	- 新增渲染正确性、公共头文件、输入、页面、窗口、进程退出、相机生命周期和帧布局测试；原有图片测试继续保留。
	- macOS Debug/Release 均完成原生构建，全部 37 个 demo 已编译并链接。
	- Release 14/14 功能测试通过；性能基准单独 1/1 通过。Debug 14/14 功能测试通过。
	- AddressSanitizer + UndefinedBehaviorSanitizer 下 14/14 功能测试通过；本轮实际发现并修复 viewport 有符号溢出和相机帧 64-byte heap overflow。macOS 不支持 LeakSanitizer，因此该证据不包含 leak gate。
	- 37/37 demo 完成 1.5 秒逐进程启动 smoke，无启动崩溃；相机 demo 延长运行可枚举设备并创建帧。
	- ccap 子模块发现 1054 个用例；本地阻断集合选择 1016 个，结果 1009 passed / 7 skipped / 0 failed。该集合与 XEGE 顶层 CTest 分开记账。

### 当前兼容边界

- Windows 的 GDI/GDI+ 默认路径未改为 OpenGL，`EGE_BUILD_OPENGL` 仍默认 OFF，旧 API 枚举值、库名和 HWND 语义保持；当前 macOS 环境仍不能替代真实 Windows 运行验证。
- Linux/Windows、MinGW cross、v141 安装、Windows OpenGL 3.3 context 和 release dry-run 均已写入 workflow，但当前分支没有 PR 或 Actions 运行记录，必须以远端首跑作为合入门禁。
- 原生 `sys_edit` 与 `inputbox_getline` 仍缺少平台控件实现。
- 原生 `ege_enable_aa` 保留兼容状态，但尚不等价于 Windows GDI+ 的完整抗锯齿质量；完整 GDI+ Path/Region/Graphics 对象仍是 Windows 专属能力。
- macOS 系统 OpenGL 已被 Apple 标记为 deprecated；当前 OpenGL 3.3 Core 实现可用，但长期可考虑增加 Metal 等后端。
- ccap 阻断集合暂不包含 8 个硬件敏感性能用例、27 个真实摄像头 CLI 用例和 3 个已确认的 orientation test-harness 误报；macOS job 也不能代表 Windows MSMF/DirectShow 或 Linux V4L2。
- 自动化验收以确定性的像素/状态测试为准，尚未建立跨平台截图人工基线；去除注释后的启发式统计为 288 个 EGEAPI 名称、117 个被测试直接引用，不能声称所有公开 API 都已有一对一单元测试。

### 当前合入判定与后续工作

当前判定为 **No-Go（本地实现候选已准备好进入 PR 验证，但尚不应直接合入）**。

1. 将预期源码、测试和 5 个 workflow 纳入提交，明确排除无关未跟踪的 `3rdparty/libpng/`、`3rdparty/zlib/`。
2. 创建 PR，取得 native、sanitizer、ccap、MSVC GDI、MSYS2/WinLibs、MinGW cross、Windows OpenGL 和 release dry-run 全绿证据。
3. 人工验证 Windows 真实相机枚举/切换/关闭与窗口 Escape/关闭/进程退出；hosted runner 无法覆盖这部分。
4. 将稳定任务设为 required checks；当前 master 没有 branch protection，workflow 存在并不等于强制门禁。
5. 后续修复 ccap 三个 orientation harness、测试路径/并行临时目录问题，并逐步扩充公开 API family 覆盖。
