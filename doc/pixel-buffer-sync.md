# 像素缓冲与 OpenGL 同步契约

本文记录 `getbuffer`、逐像素读写和贴图路径在 GDI/OpenGL 双后端下的兼容目标、
实现策略及无法隐藏的边界。这里的边界是长期契约，不是允许后端产生不同像素结果的借口。

## 兼容目标

以下行为必须与既有 Windows GDI 后端一致；不一致应视为缺陷：

- `color_t` 缓冲是从左上角开始、逐行向下的连续 ARGB32 数组，像素 `(x, y)` 位于
  `buffer[y * getwidth(image) + x]`。
- `getbuffer` 返回时能读到此前同一目标上的绘制结果；通过可写指针完成的修改在后续
  EGE 绘制、贴图、保存或读取时可见。
- CPU 修改和 GPU 绘制遵守 API 调用顺序，不得因同步优化丢失、覆盖或延迟一方的结果。
- `getbuffer(nullptr)`、`markbufferdirty(nullptr, ...)` 和 `updatebuffer(nullptr, ...)`
  操作当前绘图目标；物理缓冲坐标不叠加 viewport 原点，也不受 viewport 裁剪。
- 像素方向、颜色通道、alpha、裁剪及目标区域以自动化像素断言为准，不能列为后端差异。

Windows 仍默认构建和使用 GDI；OpenGL 是构建时启用、由应用以 `INIT_OPENGL` 选择的后端。
没有采用新接口的旧程序保持原有行为。

## 当前实现

GDI 的 `IMAGE` 直接使用 top-down DIB。`getbuffer` 在返回 DIB 地址前调用 `GdiFlush`，
使排队的 GDI 操作完成；CPU 和 GDI 实际访问同一份存储，不需要显式上传或读回。

OpenGL 的 `IMAGE` 同时维护纹理/帧缓冲和一份兼容旧接口的 CPU 镜像，并记录以下状态：

| 状态 | 含义 | 下一次必要操作 |
| --- | --- | --- |
| synchronized | CPU 镜像与 GPU 内容一致 | 无 |
| GPU newer | GPU 绘制修改了一个已知区域 | CPU 读取前只读回脏区 |
| CPU newer | 可写缓冲修改了一个已知或未知区域 | GPU 使用前只上传脏区；未知区域按全图处理 |
| screen texture newer | 窗口 back buffer 已复制到稳定纹理，CPU 尚未读取 | CPU 读取时从稳定纹理读回 |

图元批次、清屏、贴图、滤镜等 GPU 路径在完成时合并自己的目标脏区。OpenGL 读回和上传
使用 `GL_BGRA/GL_UNSIGNED_BYTE`，只对行序做上下翻转，并复用中转缓冲，避免逐像素通道
转换和每次分配。窗口在交换缓冲前保存稳定纹理，使之后读取屏幕不会误读下一帧的 back buffer。

## 三种 CPU 像素访问方式

### 兼容方式：`getbuffer`

```cpp
color_t* pixels = getbuffer(image);
pixels[y * getwidth(image) + x] = RED;
putimage(nullptr, 0, 0, image);
```

这是完全兼容旧代码的方式。OpenGL 无法观察裸指针实际写了哪些地址，因此可写
`getbuffer` 默认把整幅图标为“CPU 可能已修改”。它优先保证正确性，下一次 GPU 使用可能
上传全图。

只读代码应让实参具有 `PCIMAGE`/`const IMAGE*` 类型以选中 const 重载；该重载不会把
CPU 镜像标成已修改。仅把可写返回值当作只读指针使用仍会触发保守的全图上传。

### 兼容且可提示范围：`markbufferdirty`

```cpp
color_t* pixels = getbuffer(image);
// ...只修改 [x, x + width) × [y, y + height)...
markbufferdirty(image, x, y, width, height);
```

`markbufferdirty` 把刚才由可写 `getbuffer` 产生的“未知全图”收窄到调用者声明的物理矩形。
它不改变像素，只减少后续上传量。GDI 下它是无操作。

调用者必须满足以下条件：

- 在一次写入批次结束后、该图像上的下一次 EGE 操作前调用；
- 矩形覆盖自最近一次可写 `getbuffer` 以来的全部写入；
- 多个不相邻的小区域可以分别完成“取指针、写入、标记”批次；若很零散，合并为包围矩形
  通常比大量极小上传更实用；
- 非法或空矩形不会收窄保守脏区，因此不会为了性能提示牺牲旧代码正确性。

### 首选高频方式：`updatebuffer`

```cpp
updatebuffer(image, x, y, width, height, sourcePixels, sourcePitchBytes);
```

`updatebuffer` 明确表达“用给定 top-down ARGB32 数据覆盖这个矩形”。OpenGL 可以直接复制
到 CPU 镜像并上传该子区域，不需要先把目标纹理读回；GDI 则逐行复制到 DIB。`pitchBytes`
为 0 时表示紧密排列，非 0 时必须至少容纳一行。

对相机帧、软件解码器、模拟器帧缓冲、持续更新的小块纹理等高频生产者，优先使用
`updatebuffer`。如果算法必须读取并原地修改旧像素，使用 const `getbuffer` 读取后计算到独立
缓冲，再以 `updatebuffer` 提交；只有必须原地写时才使用可写 `getbuffer` 加
`markbufferdirty`。

## 无法完全隐藏的边界及原因

### 1. GPU 绘制后的首次 CPU 读取可能阻塞

GDI DIB 常驻 CPU 内存，而 OpenGL 绘制结果首先位于 GPU。旧 `getpixel`/`getbuffer` 是同步
返回接口，函数返回时数据必须可用，因此 GPU 尚未完成时需要等待并读回。PBO 或异步 staging
只能把等待移到稍后；在不增加“提交读取/稍后取结果”异步接口的前提下，无法保证第一次读取
完全不阻塞。

当前方案通过批量提交、精确 GPU 脏区和延迟到真正读取时再同步来降低成本。不要在逐帧热循环
中交替执行一次 GPU 绘制和一次 CPU 读取；应先完成一批绘制，再集中读取需要的区域。

### 2. 保留可写指针并跨越后续绘制再次写入不受支持

以下写法在 OpenGL 下没有可靠的自动合并方式：

```cpp
color_t* pixels = getbuffer(image);
pixels[0] = RED;
putpixel(1, 1, GREEN, image); // GPU 已在之后产生新结果
pixels[2] = BLUE;             // 通过旧指针再次写入
```

原因是普通 C++ 指针写入没有回调、范围或时刻信息。若后端把整个旧 CPU 镜像上传，会覆盖
中间的 GPU 绘制；若以后端 GPU 内容为准，又会丢失最后一次指针写入；每次 EGE 操作都全图
读回则会把兼容代价变成稳定的性能瓶颈。

正确用法是在任何可能修改图像的 EGE 操作之后重新调用 `getbuffer`，开始新的读/写批次；
或者改用 `updatebuffer`。这与公共头文件中的指针使用约束一致。

### 3. OpenGL 图像不提供可供 GDI 绘制的真实 HDC

由 OpenGL 后端创建的 GPU `IMAGE` 的 `getHDC` 返回空值；OpenGL 窗口建立前创建、因而仍由
GDI DIB 承载的旧图像可以继续返回 HDC，并通过混合后端贴图路径与 GPU 图像交换像素。

把 OpenGL 纹理伪装成可由任意 Win32 API 直接绘制的 HDC，需要额外 DIB、双向同步和外部
GDI 写入侦测，仍无法可靠得知外部写入范围。项目选择保留混合图像兼容路径，而不承诺 GPU
图像本身具备 HDC；需要原生 GDI 绘制的程序应保留 GDI 后端或显式使用 GDI 图像作为中转。

### 4. 像素缓冲访问具有图形线程约束

OpenGL context 绑定线程，`getbuffer` 可能提交绘制并调用读回；`updatebuffer` 可能上传纹理。
因此这些调用必须发生在拥有 EGE 图形 context 的线程，且不能与同一图像的绘制或缓冲访问
并发执行。为了兼容旧 API，没有在每个像素操作外增加隐式跨线程调度和锁。

### 5. 指针生命周期不因兼容而延长

图像 resize、以不同尺寸重新加载、删除，或窗口缓冲重建后，旧地址失效。这是内存重新分配的
必然结果，GDI/OpenGL 都不保证地址稳定。调用者不能缓存并在上述操作后继续使用旧指针。

## 测试与性能门禁

`rendering_correctness` 在 Windows GDI 和 Windows OpenGL 上运行同一组断言，覆盖 const/可写
`getbuffer`、屏幕/离屏图像、GPU→CPU、CPU→GPU、显式脏区、带 stride 的区域更新、错误参数、
后续绘制顺序及 GDI/OpenGL 混合图像。

`image_buffer_performance` 分别记录首次读回、缓存只读、绘制/读回循环、旧式保守全图上传、
`markbufferdirty` 区域上传、`updatebuffer` 区域上传及 GPU→GPU 复制。性能数据用于发现退化，
不以特定机器上的固定毫秒数作为功能正确性门禁。

代码评审时，任何新增 GPU 写入路径都必须标出准确脏区或保守标记全图；任何内部已知写入
范围的 CPU 路径都应使用范围化写接口。若无法证明范围正确，宁可退回全图同步，不能留下
CPU/GPU 内容不一致。

## 技术依据

- Khronos 的 [`glReadPixels` 说明](https://wikis.khronos.org/opengl/GLAPI/glReadPixels)
  规定客户端内存返回的数据按 OpenGL 的低行到高行排列，因此实现必须转换成 EGE 的
  top-down 行序。
- Khronos 的 [Pixel Buffer Object 指南](https://wikis.khronos.org/opengl/Pixel_Buffer_Object)
  说明 PBO 可以把同步推迟到应用访问缓冲时，但若 `glReadPixels` 后立即映射读取，就没有
  可用于隐藏传输延迟的并行工作。这正是旧同步 `getbuffer` 不能仅靠 PBO 消除等待的原因。
- Khronos 的 [Synchronization 说明](https://wikis.khronos.org/opengl/Synchronization)
  明确区分异步渲染命令与返回时必须已经填好客户端内存的非 PBO `glReadPixels`。因此当前
  实现选择按需读回和缩小脏区；未来若增加异步读取，应作为独立接口，而不能改变
  `getbuffer` 的返回语义。
