# 性能诊断设计

本文定义 EGE 对“结果正确但通常低效”的兼容行为如何提示。诊断用于帮助开发者定位
CPU/GPU 同步瓶颈，不改变 API 返回值、像素结果、调用顺序或后端选择，也不把合法的旧接口
降级为错误。

## 设计原则

- 只报告后端能够确认已经发生的代价，不根据函数名或调用次数猜测。例如重复读取已同步的
  CPU 缓冲不会被计为 GPU 回读。
- 正常的首次同步不提示；只有短时间内重复整图上传或重复读回等可行动模式才提示。
- 每个稳定诊断编号在一个进程中最多记录一次，避免逐帧刷屏；不同图像共享该去重范围。
- GDI 和 OpenGL 保持相同功能结果。OpenGL 专有代价只在实际使用 OpenGL 运行时报告。
- 诊断代码不调用 EGE 绘图接口，不改变同步状态，也不在 Release 默认热路径中保留计时统计。

## 当前诊断编号

| 编号 | 触发条件 | 不会触发的情况 | 建议 |
| --- | --- | --- | --- |
| `EGE-PERF-001` | 同一个持久 CPU Bitmap 在 1000 ms 窗口内发生第 3 次整图 CPU→GPU 上传 | 第 1/2 次上传、GPU IMAGE 的内部精确脏区或 `updatebuffer`、GDI | 合并裸指针/HDC 写入并尽量减少采样，需要稳定存储时至多每帧一次；否则改用 `updatebuffer` |
| `EGE-PERF-002` | 同一个 OpenGL 渲染目标在 1000 ms 窗口内发生第 3 次像素缓冲访问所需的真实 GPU→CPU 读回 | 单次读回、同步后仅访问 CPU 缓存、其他不经过像素 shadow 的内部读回、GDI | 批量完成 GPU 绘制后集中读取，避免在循环中交替绘制和读取 |

阈值是性能启发式，不是正确性边界。即使产生诊断，旧接口仍按兼容语义执行；不同驱动和
图像尺寸的实际耗时应由 `image_buffer_performance` 与 `pixel_access_performance` 测量。

## 输出渠道

启用时始终先写一条结构化 stderr 日志：

```text
[EGE][performance][EGE-PERF-001] OpenGL IMAGE 1920x1080: ...
```

- 交互式终端中的性能提示使用黄色；红色保留给错误或可能产生错误结果的行为。
- stderr 被重定向、终端不可识别或设置了 `NO_COLOR` 时输出纯文本，不写 ANSI 转义序列。
- Windows Debug 同时写入 `OutputDebugString`，Visual Studio/调试器无需控制台也能看到详情。

仅当显式设置 `EGE_DIAGNOSTICS=all` 或 `popup` 时，Windows Debug 才会在进程首次产生性能
诊断时尝试显示一个 EGE 自有提示窗。它使用
`WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`，不抢焦点、不阻塞调用线程，8 秒后自动关闭；详细修复
建议仍以日志为准。以下场景不创建提示窗：

- `INIT_HIDE` 或原生窗口当前不可见；
- EGE 窗口嵌入其他宿主，是一个 `WS_CHILD`；
- 触发线程不是窗口所有线程；
- 非 Windows、非 Debug，或运行时选择了仅日志/关闭模式。

提示窗创建失败只会放弃 UI 渠道，不影响日志、绘制和程序退出。

## 构建与运行控制

CMake 缓存项 `EGE_PERFORMANCE_DIAGNOSTICS` 接受以下值：

| 值 | 行为 |
| --- | --- |
| `AUTO` | 默认值；Debug 编译诊断，Release/RelWithDebInfo/MinSizeRel 编译掉诊断热路径 |
| `ON` | 所有配置编译日志诊断；非 Debug 仍不显示提示窗 |
| `OFF` | 所有配置关闭 |

诊断已编译时，进程启动前可设置 `EGE_DIAGNOSTICS`：

| 值 | 行为 |
| --- | --- |
| 未设置、`log` 或 `stderr` | stderr 和 Windows Debug 调试器输出，不显示提示窗 |
| `all` 或 `popup` | 保留日志和调试器输出，并在满足条件时显示首次提示窗 |
| `off`、`false` 或 `0` | 完全关闭性能诊断 |

运行时开关只影响提示，不改变同步策略。需要稳定测试输出的自动化程序应使用
`EGE_DIAGNOSTICS=log` 或 `off`；隐藏窗口测试不会弹窗。

## 可检测边界

普通 C++ 裸指针和 HDC 没有写入回调，因此后端不能知道调用者是否真的修改过它们，也不能
知道具体修改范围。为兼容旧接口，持久 CPU Bitmap 在每次被 OpenGL 采样时仍保守上传整图，
包括后续 EGE 绘制之后通过保留指针继续写入的合法用法。`EGE-PERF-001` 只能确认桥接上传已
重复发生，不能声称检测到了实际用户写入；详见 [`pixel-buffer-sync.md`](pixel-buffer-sync.md)。

当前没有新增公共诊断回调。内部一次性日志足以覆盖现有像素慢路径，也避免引入回调线程、
重入和 ABI 契约；若未来确有 IDE 或引擎集成需求，应另行设计带编号、严重级别和计数的回调，
并明确禁止在回调中重入绘图接口。

## 自动化验证

`performance_diagnostics` 在 GDI、OpenGL、运行时关闭以及 Debug/Release 编译策略下验证编号、
去重、持久 CPU Bitmap 重复上传、真实 GPU 重复读回、`updatebuffer`/缓存读取误报边界和无颜色
重定向输出。Windows OpenGL Debug 的同一测试程序还接受 `--popup-smoke`，用于本地短暂显示
真实窗口并程序化确认提示窗属于 EGE 主窗口、带有
`WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`、不会改变前台焦点；该可见 smoke 不注册为默认 CTest，
避免 hosted/headless 测试创建交互式 UI。
