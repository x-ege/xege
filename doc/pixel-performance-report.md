# Windows 像素接口双后端测试报告（2026-07-21）

## 结论

本轮 Windows GDI/OpenGL Release 正确性回归全部通过，像素接口家族没有发现未解决的结果
差异。持久 CPU Bitmap 方案达到了兼容目标：保留裸指针后的后续写入能够被 OpenGL 上屏观察，
纯 CPU 缓存读取与 GDI 基本持平。OpenGL 额外成本主要来自必须的 GPU 回读和整图上传，而不是
CPU Buffer 本身。

性能基准发现并修复了一个独立的 GDI 热点：`getpixel_f` 原先通过 const `getbuffer` 每像素
执行 `GdiFlush`。修复前的探索性采样约为 18.36 ms/20 万次，修复后的五进程汇总为
0.2602 ms/20 万次，约提升 70 倍，并与普通 GDI `getpixel` 对齐。

当前方案适合兼容优先的发布。应用应避免 GPU 写入后逐像素立即读取；重复 CPU 写入应集中在
CPU Bitmap 中，并把上屏次数控制为每帧一次。没有必要为了当前正确性再强制增加脏区接口，
但若未来实际项目证明 4K 或多张动态 CPU Bitmap 的整图上传成为瓶颈，可在保持默认保守上传的
前提下增加可选显式脏区提示。

## 环境与方法

- Windows 11 Enterprise 10.0.26200，MSVC 19.44，Release。
- Intel Core i9-14900K（24 核/32 线程）。
- NVIDIA GeForce RTX 4090 D，驱动 32.0.15.6094；同时安装 Intel UHD Graphics 770。
- 图像为 1024×1024 BGRA（单张 4 MiB）。
- 每项 3 次预热、21 个计时样本；GDI/OpenGL 各运行 5 个独立进程，奇偶轮交换启动顺序。
- 表中数据是“五个进程各自中位数”的中位数。计时体外完成 fixture 重置；计时后验证像素、
  指针和存储模式，任何正确性错误都会使进程失败。
- 机器相关时间不作为 CTest 门禁阈值。原始日志、逐轮数据、汇总和环境快照位于
  `build/pixel-performance-results/cpu-bitmap-final/`。

## 性能结果

### 读取、同步与存储转换

| 场景 | 每样本操作数 | GDI 中位数 | OpenGL 中位数 | OpenGL/GDI |
| --- | ---: | ---: | ---: | ---: |
| `getpixel_cached` | 200,000 | 0.2639 ms | 1.2110 ms | 4.59× |
| `getpixel_f_cached` | 200,000 | 0.2602 ms | 0.6499 ms | 2.50× |
| `cpu_bitmap_getpixel_cached` | 200,000 | 0.2409 ms | 0.2552 ms | 1.06× |
| `getbuffer_read_first` | 1 | 0.0004 ms | 2.6581 ms | 6645×* |
| `getbuffer_read_write_promotion` | 1 | 0.0004 ms | 3.1756 ms | 7939×* |
| `getbuffer_write_discard_promotion` | 1 | 0.0004 ms | 0.9600 ms | 2400×* |
| `getbuffer_read_after_draw_cycle` | 32 | 0.0033 ms | 72.7288 ms | 22039×* |
| `putpixel_getpixel_cycle` | 32 | 0.0002 ms | 73.8214 ms | 369107×* |
| `cpu_bitmap_retained_1px_upload_cycles` | 20 | 2.2342 ms | 9.1350 ms | 4.09× |

`*` GDI 基线接近计时器分辨率，倍率不适合单独解读，应看绝对耗时。OpenGL 的
`WRITE_DISCARD` 比 `READ_WRITE` 首次转换少约 2.22 ms（约 70%），验证了跳过 GPU 回读的
价值。GPU 写后立即读的两个 32 次循环约为 2.27–2.31 ms/同步点，代表刻意构造的最差访问
模式，不代表正常批量渲染吞吐。

### 像素写入并提交

| 场景 | 每样本操作数 | GDI 中位数 | OpenGL 中位数 | OpenGL/GDI |
| --- | ---: | ---: | ---: | ---: |
| `putpixel_committed` | 20,000 | 0.0225 ms | 3.4996 ms | 155.54× |
| `putpixel_f_staging_committed` | 20,000 | 1.9909 ms | 3.4026 ms | 1.71× |
| `putpixels_staging_committed` | 20,000 | 0.1606 ms | 3.3208 ms | 20.68× |
| `putpixels_f_staging_committed` | 20,000 | 0.1608 ms | 3.3157 ms | 20.62× |
| `putpixel_withalpha_staging_committed` | 10,000 | 1.1137 ms | 3.3781 ms | 3.03× |
| `putpixel_withalpha_f_staging_committed` | 10,000 | 1.0872 ms | 3.3075 ms | 3.04× |
| `putpixel_savealpha_staging_committed` | 10,000 | 1.0756 ms | 3.3043 ms | 3.07× |
| `putpixel_savealpha_f_staging_committed` | 10,000 | 1.0575 ms | 3.3121 ms | 3.13× |
| `putpixel_alphablend_staging_committed` | 10,000 | 1.0971 ms | 3.3439 ms | 3.05× |
| `putpixel_alphablend_f_staging_committed` | 10,000 | 1.0929 ms | 3.3443 ms | 3.06× |
| `putpixel_alphablend_factor_staging_committed` | 10,000 | 1.0982 ms | 3.3927 ms | 3.09× |
| `putpixel_alphablend_factor_f_staging_committed` | 10,000 | 1.1150 ms | 3.3992 ms | 3.05× |
| `cpu_bitmap_putpixel_committed` | 20,000 | 0.1471 ms | 3.3633 ms | 22.86× |
| `cpu_bitmap_retained_writes_committed` | 20,000 | 0.1322 ms | 3.3288 ms | 25.18× |

绝大多数 OpenGL 批量写入结果聚集在 3.3–3.5 ms，说明一批 1 万或 2 万次 CPU 像素操作
只在提交阶段承担一次同步/上传，而不是每个像素都上传。CPU Bitmap 保留指针写入和通过 API
写入的提交耗时也基本一致；两者共同成本是兼容裸指针所要求的 4 MiB 整图上传。

## 正确性与覆盖结果

| 检查 | 结果 |
| --- | --- |
| Windows GDI Release 全量 CTest | 21/21 通过，12.34 s；包含全部性能标签用例 |
| Windows OpenGL Release 全量 CTest | 37/37 通过，91.64 s；包含 GDI 基线和全部 `_opengl` 变体 |
| 像素接口专项 | `getpixel/getpixel_f`、`putpixel/putpixel_f`、`putpixels/putpixels_f`、with-alpha、save-alpha、alpha-blend 及其 `_f`/显式 alpha-factor 重载均有结果断言和性能采样 |
| `getbuffer` 专项 | const、兼容可写重载、READ/READ_WRITE/WRITE_DISCARD、存储查询/转换、首次/缓存读回、保留指针、每次上屏、CPU↔GPU 桥均覆盖 |
| 公共 API 静态审计 | 两份头文件函数名 290/290、声明 385/385；直接测试引用 290/290 |
| Release demos 构建 | GDI/OpenGL 两套 `demos` 目标均成功 |
| 固定帧截图对比 | `graph_alpha`、`graph_ball`、`test_demo` 均通过；MAE 分别为 0.00491、0.39016、0.64298，均低于 2.0 阈值 |

## 已知边界

- 本报告性能数字只代表这台 Windows/NVIDIA 机器；驱动、PCIe、集显和虚拟机环境可能明显不同。
- Windows 测试不能证明 macOS/Linux 的运行时路径；对应行为仍应由各平台 CI 验证。
- 裸指针在图形线程外并发写入，以及取得 `IMAGE_BUFFER_READ` 后违反承诺写入，均不在接口支持范围。
- 默认整图上传是无法观察裸指针何时被修改所导致的兼容性成本。当前没有脏矩形追踪，也没有把
  用户必须调用“已修改”接口作为正确性的前提。
- 三个固定帧截图覆盖代表性画面，但不替代所有交互式 demo 的全部输入和状态组合。
