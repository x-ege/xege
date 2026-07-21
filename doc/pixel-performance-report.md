# Windows 像素接口双后端测试报告（2026-07-21）

## 结论

本轮选择了两条分支的混合方案：以 `feature/opengl-cpu-bitmap` 的持久裸指针兼容语义作为
正确性主线，吸收 `feature/pixel-buffer-sync-performance` 的 GPU 脏矩形和精确区域更新思想。
没有直接 merge/rebase 后者，因为它依赖调用方标记可写缓冲区，无法保证旧程序保留指针后再次
写入仍被观察到。

Windows GDI/OpenGL Release 正确性回归全部通过。公共可写 `getbuffer` 与 `getHDC` 会把 OpenGL
IMAGE 提升为持久 CPU Bitmap，继续保证旧指针/HDC 写入正确；只读缓冲、EGE 像素 API 和新增
`updatebuffer` 则保持 GPU 存储。这样把“未知裸指针写入”和“库已知精确修改范围”分开处理，
避免为了兼容低频接口让所有像素操作都承担整图回读/上传。

相对 CPU Bitmap 主线的同机历史数据，32 次 GPU 绘制后读循环从 72.7288 ms 降至 0.8909 ms
（约 81.6 倍），32 次 `putpixel` 后立即 `getpixel` 从 73.8214 ms 降至 1.1108 ms（约 66.5 倍）。
区域批量写入通常约快 2 倍。持久 CPU Bitmap 的每次采样仍保守上传整图，这是旧裸指针兼容的
必要成本；全帧 `updatebuffer` 也仍受传输带宽限制。

## 环境与方法

- Windows 11 Enterprise 10.0.26200，MSVC 19.44，Release。
- Intel Core i9-14900K（24 核/32 线程）。
- NVIDIA GeForce RTX 4090 D，驱动 32.0.15.6094；同时安装 Intel UHD Graphics 770。
- 图像为 1024×1024 BGRA（单张 4 MiB）。
- 每项 3 次预热、21 个计时样本；GDI/OpenGL 各运行 5 个独立进程，奇偶轮交换启动顺序。
- 表中数据是“五个进程各自中位数”的中位数。计时体外完成 fixture 重置；计时后验证像素、
  指针和存储模式，任何正确性错误都会使进程失败。
- 机器相关时间不作为 CTest 门禁阈值。原始日志、逐轮数据、汇总和环境快照位于
  `build/pixel-performance-results/hybrid-final/`；环境快照记录提交 `0109747`。

## 性能结果

### 读取、同步与存储转换

| 场景 | 每样本操作数 | GDI 中位数 | OpenGL 中位数 | OpenGL/GDI |
| --- | ---: | ---: | ---: | ---: |
| `getpixel_cached` | 200,000 | 0.2630 ms | 1.0201 ms | 3.88× |
| `getpixel_f_cached` | 200,000 | 0.2606 ms | 0.7103 ms | 2.73× |
| `cpu_bitmap_getpixel_cached` | 200,000 | 0.2544 ms | 0.2666 ms | 1.05× |
| `getbuffer_read_first` | 1 | 0.0009 ms | 0.9041 ms | 1005×* |
| `getbuffer_read_write_promotion` | 1 | 0.0009 ms | 1.4112 ms | 1568×* |
| `getbuffer_write_discard_promotion` | 1 | 0.0008 ms | 0.8227 ms | 1028×* |
| `getbuffer_read_after_draw_cycle` | 32 | 0.0105 ms | 0.8909 ms | 84.85×* |
| `putpixel_getpixel_cycle` | 32 | 0.0002 ms | 1.1108 ms | 5554×* |
| `cpu_bitmap_retained_1px_upload_cycles` | 20 | 2.8205 ms | 16.9762 ms | 6.02× |

`*` GDI 基线接近计时器分辨率，倍率不适合单独解读，应看绝对耗时。`WRITE_DISCARD` 比
`READ_WRITE` 首次转换少 0.5885 ms（约 42%），验证了跳过 GPU 回读的价值。区域追踪显著缩小
了数据量，但 GPU 写后立即 CPU 读仍是强制同步点，不应把这一压力场景当作正常批量吞吐。

### 像素写入并提交

| 场景 | 每样本操作数 | GDI 中位数 | OpenGL 中位数 | OpenGL/GDI |
| --- | ---: | ---: | ---: | ---: |
| `putpixel_committed` | 20,000 | 0.0227 ms | 2.9307 ms | 129.11× |
| `putpixel_f_staging_committed` | 20,000 | 6.3552 ms | 1.7732 ms | 0.28× |
| `putpixels_staging_committed` | 20,000 | 0.1766 ms | 1.6639 ms | 9.42× |
| `putpixels_f_staging_committed` | 20,000 | 0.1761 ms | 1.6576 ms | 9.41× |
| with/save-alpha/alpha-blend 家族 | 10,000 | 3.1110–3.3467 ms | 1.6744–1.7438 ms | 0.50–0.54× |
| `cpu_bitmap_putpixel_committed` | 20,000 | 0.1665 ms | 1.6664 ms | 10.01× |
| `cpu_bitmap_retained_writes_committed` | 20,000 | 0.1520 ms | 1.6132 ms | 10.61× |
| `updatebuffer_1px_committed` | 32 | 3.6738 ms | 2.0199 ms | 0.55× |
| `updatebuffer_64x64_committed` | 32 | 4.2556 ms | 2.4403 ms | 0.57× |
| `updatebuffer_full_frame_committed` | 8 | 2.1896 ms | 8.4451 ms | 3.86× |

OpenGL 的内部像素家族现在把一批修改合并为精确像素或包围矩形，只在提交阶段上传一次。
`updatebuffer` 的 1 像素和 64×64 场景无需目的纹理回读，在本机快于 GDI；这不是跨机器性能
承诺。全帧场景每轮传输 4 MiB，OpenGL 明显慢于内存内的 GDI。CPU Bitmap 保留指针写入与
通过 API 写入的 OpenGL 提交耗时接近，二者都承担兼容语义要求的整图上传。

## 正确性与覆盖结果

| 检查 | 结果 |
| --- | --- |
| Windows GDI Release 功能/性能 | 功能 12/12（3.47 s），性能 9/9（11.34 s），合计 21/21 |
| Windows OpenGL Release 功能/性能 | 功能 19/19（6.28 s），性能 18/18（103.54 s），合计 37/37；包含 GDI 基线和 `_opengl` 变体 |
| 像素接口专项 | `getpixel/getpixel_f`、`putpixel/putpixel_f`、`putpixels/putpixels_f`、with-alpha、save-alpha、alpha-blend 及其 `_f`/显式 alpha-factor 重载均有结果断言和性能采样；内部路径保持 GPU 存储 |
| `getbuffer`/`getHDC` 专项 | const、兼容可写重载、READ/READ_WRITE/WRITE_DISCARD、存储查询/转换、首次/缓存读回、保留指针/HDC、每次上屏、CPU↔GPU 桥均覆盖 |
| `updatebuffer` 专项 | 紧密/带 padding stride、1 像素/64×64/全帧、IMAGE/屏幕目标、物理坐标、viewport 独立性、非法区域/空指针/stride 和存储模式保持均覆盖 |
| 公共 API 静态审计 | 两份头文件函数名 291/291、声明 386/386；直接测试引用 291/291，声明参数个数证据 386/386 |
| Release demos 构建 | GDI/OpenGL 两套 `demos` 目标均成功 |
| 固定帧截图对比 | `graph_alpha`、`graph_ball`、`test_demo` 均通过；MAE 分别为 0.00491、0.39016、0.64270，均低于 2.0 阈值 |

## 已知边界

- 本报告性能数字只代表这台 Windows/NVIDIA 机器；驱动、PCIe、集显和虚拟机环境可能明显不同。
- Windows 测试不能证明 macOS/Linux 的运行时路径；对应行为仍应由各平台 CI 验证。
- 裸指针在图形线程外并发写入，以及取得 `IMAGE_BUFFER_READ` 后违反承诺写入，均不在接口支持范围。
- GPU 路径会追踪内部操作的脏矩形；持久 CPU Bitmap 的默认整图上传仍是无法观察裸指针/HDC
  何时被修改所导致的兼容成本。没有把用户必须调用“已修改”接口作为正确性的前提。
- GPU IMAGE 当前预留整图 CPU shadow；CPU Bitmap 的 OpenGL 采样桥还保留可复用 render target。
  可进一步做延迟 staging 分配以降低内存，但不能牺牲指针稳定性。
- 三个固定帧截图覆盖代表性画面，但不替代所有交互式 demo 的全部输入和状态组合。
