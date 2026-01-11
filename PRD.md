# EGE 跨平台重构 PRD（OpenGL 后端）

## 背景与问题

EGE（Easy Graphics Engine）目前以 Windows 的 GDI/GDI+ 渲染链路为主。项目虽然“名义上跨平台”，但在 Linux/macOS 上通常依赖 **wine + mingw-w64 交叉编译**才能工作，无法称为真正的原生跨平台。

本 PRD 的目标是：在**尽量保持主干行为不变**的前提下，引入一个可选的 OpenGL 渲染后端，并在开启该选项时实现 Linux/macOS 的 **native 原生构建与运行**，从而逐步废弃 wine 那套“伪跨平台”路径。

## 总体目标（Goals）

1. 引入 OpenGL 渲染后端，使 EGE 在 Windows / Linux / macOS 具备跨平台渲染能力。
2. 通过模块化/抽象层拆分窗口与绘制后端，保证未来可扩展更多后端。
3. 以 `demo/` 作为主要兼容性回归集：第一阶段目标至少做到 **能编译**，并逐步做到 **能运行**。

## 非目标（Non-Goals）

- 不在第一阶段强求 OpenGL 路径完全复刻 GDI/GDI+ 的像素级一致性。
- 不在第一阶段完成所有高级特性（复杂路径、完整文字栅格化、所有混合模式等）。优先保证核心 API 的可用性与稳定性。

## 构建开关：EGE_BUILD_OPENGL（关键约束）

**唯一入口：**在 CMake 中新增/使用 `EGE_BUILD_OPENGL` 选项来控制是否启用 OpenGL 跨平台模式。

**默认策略（对源码仓库/开发者而言）：**

- `EGE_BUILD_OPENGL=OFF` 视为“与主干一致”的默认路径（不引入 OpenGL 依赖、不暴露 OpenGL init 选项、Linux/macOS 继续走 mingw-w64 + wine 的交叉编译/运行模式）。
- `EGE_BUILD_OPENGL=ON` 进入“真正跨平台”路径（Linux/macOS native + 强制 OpenGL；Windows 仍默认旧后端，OpenGL 需 opt-in）。

> 说明：仓库内可能还存在其它历史/兼容选项（例如 `EGE_BUILD_FOR_LINUX`）。本 PRD 以 `EGE_BUILD_OPENGL` 作为跨平台重构的唯一开关入口，其它选项在开启 OpenGL 时要被约束/收敛为一致行为（见下文）。

### 当 `EGE_BUILD_OPENGL=OFF`（默认）

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

- 当 `EGE_BUILD_OPENGL=OFF`：Linux/macOS 默认不启用 native 构建（应继续 cross-compile Windows 目标并在运行时使用 wine），以对齐主干。
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
- 渲染策略（第一阶段）：允许采用更易落地的最小实现（例如 OpenGL legacy / immediate-mode），后续再逐步演进。

## 设计与架构（需要明确的抽象边界）

为支持多后端，渲染相关部分需要抽象出：

1. Window 层：负责窗口创建、事件处理、交换缓冲等。
2. GraphicsContext 层：负责基础绘制能力（像素、线、矩形、圆/椭圆等）。

并提供至少两套实现：

- Windows：GDI（现有）/（可选）OpenGL
- Linux/macOS：OpenGL（强制）

## 兼容性与验收标准

### 编译维度验收（必须）

- `EGE_BUILD_OPENGL=OFF`：Windows/Linux/macOS 的构建行为与主干一致（尤其是 API 暴露与依赖关系）。
- `EGE_BUILD_OPENGL=ON`：
  - Windows：可在不传 `INIT_OPENGL` 的情况下继续使用旧后端构建/运行；传入后走 OpenGL。
  - Linux/macOS：可 native 构建，且能运行至少一个 demo 作为 smoke test。

### 运行维度验收（阶段性）

- 第一阶段：至少跑通基础窗口、清屏、基本绘制与帧循环。
- 后续阶段：逐步补齐图片绘制、输入事件一致性、更多 demo 的运行正确性。

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

2. **Linux/macOS 发布包（过渡期双轨）**
	- 发布两个版本：
	  - A) `EGE_BUILD_OPENGL=ON`：强制 OpenGL，native 构建（真正跨平台）。
	  - B) `EGE_BUILD_OPENGL=OFF`：沿用 mingw-w64 + wine，保持与主干一致（过渡/兼容用途）。
	- 随时间逐步废弃 B 版本，最终 Linux/macOS 仅保留 A（OpenGL）版本。