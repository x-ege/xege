# 像素缓冲与 CPU/GPU 同步

## 兼容目标

旧版 Windows EGE 的 `IMAGE` 是 DIB：`getbuffer` 返回的指针直接指向图像的权威像素，
用户可以长期保留该指针，并在任意两次 EGE 操作之间继续写入。OpenGL 的普通纹理无法
可靠检测裸指针何时再次被修改，因此仅使用一次性暂存缓冲区无法完全兼容这个行为。

Windows OpenGL 后端现在有两种图像存储：

- `IMAGE_STORAGE_GPU`：OpenGL render target 保存权威像素，适合主要由 EGE/GPU 绘制的图像。
- `IMAGE_STORAGE_CPU_BITMAP`：Win32 DIB 保存权威像素，适合 `getbuffer`、软件绘制、相机帧和
  其他反复写内存的场景。

GDI 图像天然属于 `IMAGE_STORAGE_CPU_BITMAP`。Windows OpenGL 图像默认仍是 GPU 存储，
因此没有使用可写缓冲区的既有程序不会承担额外上传成本。

当前采用的是混合策略，而不是让所有像素接口都走 CPU Bitmap：

- 公共可写 `getbuffer` 和 Windows `getHDC` 需要兼容可长期保留的裸指针/句柄，因此会把
  OpenGL IMAGE 提升为持久 CPU Bitmap。
- `getpixel`、const `getbuffer` 和显式 `IMAGE_BUFFER_READ` 保持 GPU 存储，并复用按需回读缓存。
- EGE 自身已知修改范围的 `putpixel*`/`putpixels*` 路径以及公共 `updatebuffer` 保持 GPU 存储，
  只同步确切或保守包围的矩形。

## `getbuffer` 访问方式

| 调用 | 旧像素 | Windows OpenGL 存储变化 | 用途 |
|---|---|---|---|
| `getbuffer(PCIMAGE)` | 同步后可读 | 不变化 | 兼容只读访问 |
| `getbuffer(PIMAGE, IMAGE_BUFFER_READ)` | 同步后可读 | 不变化 | 明确承诺不写，避免转换 |
| `getbuffer(PIMAGE)` | 保留 | 转成 CPU Bitmap | 旧接口；默认读写，保证兼容 |
| `getbuffer(PIMAGE, IMAGE_BUFFER_READ_WRITE)` | 保留 | 转成 CPU Bitmap | 显式读写 |
| `getbuffer(PIMAGE, IMAGE_BUFFER_WRITE_DISCARD)` | 丢弃 | 转成 CPU Bitmap | 将覆盖所需像素，避免首次 GPU 回读 |

`IMAGE_BUFFER_READ` 虽因历史 ABI 返回 `color_t*`，但写入该指针不受支持。使用
`IMAGE_BUFFER_WRITE_DISCARD` 后，调用方必须在任何读取或绘制前初始化所有会被读取的像素。

也可以用 `setimagestoragemode(image, IMAGE_STORAGE_CPU_BITMAP)` 提前完成保留内容的转换，
并用 `getimagestoragemode` 查询。CPU Bitmap 不会原地转回 GPU：这种转换会使用户仍持有的
指针失效，因此返回 `grInvalidMode`；需要纯 GPU 图像时，应新建图像并复制内容。

Windows 上对 GPU IMAGE 调用 `getHDC` 也会执行同样的保留内容转换。转换后的 HDC 与 DIB
共享权威像素，后续 `SetPixel`、GDI 和 GDI+ 写入会在下一次 OpenGL 采样时被观察到。OpenGL
窗口目标不能转换为离屏 DIB，没有 Win32 CPU 绘图后端的原生平台也会返回 `NULL`。

## 精确区域更新

当调用方知道修改矩形且不需要长期保留裸指针时，应使用：

```cpp
int updatebuffer(PIMAGE image, int x, int y, int width, int height,
                 const color_t* pixels, int pitchBytes = 0);
```

源像素按自上而下 ARGB 行排列；`pitchBytes == 0` 表示紧密行。目标坐标是 IMAGE 的物理像素
坐标，不叠加 viewport 原点，也不受 viewport clip 影响。成功调用不会回读目标纹理，也不会
把 GPU IMAGE 提升为 CPU Bitmap；OpenGL 使用 `glTexSubImage2D` 上传确切矩形，窗口目标还会
把同一区域更新到当前后缓冲。空矩形、越界矩形、空源指针和短/负 stride 会返回错误。

EGE 内部的 `putpixel_f`、with-alpha/save-alpha/alpha-blend 家族会先按需回读一个像素，再只把
该像素标为 CPU 新数据；`putpixels` 家族使用所有有效点的包围矩形。它们不调用公共可写
`getbuffer`，所以 OpenGL IMAGE 不会因为普通像素 API 自动转为 CPU Bitmap。

## 同步规则

GPU 图像会跟踪自上次 CPU 同步以来由图元、贴图、清屏等操作影响的矩形。第一次只读访问会
提交待处理绘制并只回读该矩形；只要 GPU 内容没有再次变化，后续 `getpixel`、const
`getbuffer` 和 `IMAGE_BUFFER_READ` 复用 CPU 缓存，不会按调用次数重复回读。文字等难以安全
计算边界的路径使用整图脏区，宁可多传输也不缩小正确性范围。

CPU Bitmap 转换时只回读一次（写入丢弃除外）。转换后，Win32 DIB 同时服务于用户指针、
GDI、GDI+ 和 EGE 像素接口，因此不存在两份 CPU 数据相互覆盖的问题。它作为 GPU 图像源或
当前可视页上屏时，后端会在每次使用前执行完整纹理上传。这是有意采用的保守策略：即使
用户从未再次调用 EGE，也能观察到通过旧指针完成的新写入。

转换还会迁移颜色、线型、填充、字体、文字背景、viewport/clip、当前位置和写模式；已经
创建的 GDI+ 仿射变换与多态画刷（包括渐变和纹理画刷）也会保留。也就是说，转换只改变
权威像素存储，不应重置后续绘图可观察到的 IMAGE 状态。

完整上传使用可复用的行翻转缓冲区，并以 `GL_BGRA`/`glTexSubImage2D` 直接匹配 Windows
`color_t` 的内存布局。稳定尺寸下不会逐帧重新分配传输内存，也不再逐像素交换颜色通道；
传输前后会保存并恢复 PBO 绑定以及 pack/unpack 的对齐、行长和跳过参数，避免调用方的原生
OpenGL 像素状态改变 EGE 的内存布局。但上传带宽仍是整张图的 `width × height × 4` 字节，
这是裸指针兼容所选择的明确成本。

同步矩阵如下：

| 源 | 目标 | 路径 |
|---|---|---|
| GPU | GPU | render target 直接复制/采样 |
| CPU Bitmap | GPU | 每次从 DIB 刷新上传缓存，再由 GPU 采样 |
| CPU Bitmap | CPU Bitmap | GDI/GDI+ 或 CPU 像素路径 |
| GPU | CPU Bitmap | 必要时同步回读，再执行 CPU/GDI 路径 |

非 `SRCCOPY` 的三元光栅操作和三图像 alpha filter 仍可能使用 CPU 正确性路径，因而读取 GPU
目标时会产生同步等待。这些是冷门兼容接口，不影响普通纹理复制的快速路径。

## 性能使用原则

- 连续只读 GPU 图像时，第一次 `getpixel`、const `getbuffer` 或 `IMAGE_BUFFER_READ` 完成
  脏区回读，后续读取复用缓存。不要在 GPU 绘制和 CPU 读取之间逐像素交替；区域追踪能缩小
  传输量，但每次交替仍会形成必须完成的 GPU/CPU 同步点。
- 已知外部像素矩形时优先使用 `updatebuffer`。它不回读目标，也不改变存储模式，适合视频帧
  分块、软件栅格器的已知 tile 或少量动态区域；若每帧覆盖整张图，仍需支付整帧传输带宽。
- 反复由 CPU 修改的图像应转为 `IMAGE_STORAGE_CPU_BITMAP`，保留一次取得的指针并集中写入；
  每帧只在实际采样或上屏时提交。CPU 侧读取性能与 GDI DIB 相当，但 OpenGL 上屏仍需传输
  整张图。
- 完全覆盖旧内容时使用 `IMAGE_BUFFER_WRITE_DISCARD`，可跳过首次 GPU 回读；需要保留旧内容
  才使用 `IMAGE_BUFFER_READ_WRITE`。兼容无参数重载继续等价于读写访问。
- DIB 上的 `getpixel_f` 直接读取权威 `m_pBuffer`，不再为每个像素调用 `GdiFlush`；GPU
  render target 仍通过只读同步缓冲读取，不会把一次物理像素查询误标记为待上传写入。

专项数据由 `pixel_access_performance` 采集，并可用
`tests/tools/run_pixel_performance_comparison.ps1` 在 Windows 上交替运行 GDI/OpenGL。基准只把
正确性作为通过条件，不使用与 CPU、GPU、驱动相关的时间阈值。

## 指针与线程边界

- 指针在图像 `resize`、重新加载为不同尺寸、删除，或窗口页缓冲重建后失效；之后必须重新获取。
- 图像访问和 OpenGL 上下文仍要求在图形线程串行执行，不支持并发读写同一图像。
- GPU 存储会追踪内部操作的脏矩形；持久 CPU Bitmap 仍在每次采样时上传整图，因为库无法
  观察旧裸指针或 HDC 在何时、何处被再次修改。没有增加公共 `markbufferdirty`：若把它作为
  正确性条件会破坏旧接口兼容，而把它仅作为提示又无法排除调用后通过保留指针继续写入。
- GPU IMAGE 当前预留一张完整 CPU shadow 用于延迟回读；CPU Bitmap 的 OpenGL 采样桥还保留
  一份可复用的 GPU render target。这是每张 1024×1024 图约 4 MiB 的额外 CPU 内存边界，
  后续可通过延迟分配 staging 优化，但不能改变指针稳定性。
- macOS/Linux 目前没有完整的 CPU 绘图后端，因此显式 GPU→CPU Bitmap 转换返回
  `grInvalidMode`，兼容可写 `getbuffer` 仍使用既有 OpenGL 暂存缓冲语义。只读访问和 GPU 缓存
  行为在各平台一致。

## 自动化覆盖边界

Windows 回归同时运行 GDI 和 OpenGL 模式，覆盖访问意图、显式转换、`getHDC` 自动提升、
精确 `updatebuffer`、普通及 GDI+ 增强状态
（仿射变换和渐变画刷）与 resize 迁移、
保留指针跨 EGE 绘制继续写入、可视页上屏，以及 CPU Bitmap→GPU 和 GPU→CPU Bitmap 两个
方向。贴图桥覆盖基础/拉伸复制、透明色、Alpha/Alpha-transparent/with-alpha、alpha filter、
旋转、增强贴图、纹理填充和 `getimage`；文件解码还验证内部写入不会误触发公共可写缓冲转换。

无法在 Windows 机器上自动证明的边界是 macOS/Linux 的运行时行为、第三方代码在图形线程外
并发写裸指针，以及用户在 `IMAGE_BUFFER_READ` 承诺只读后仍写入。前者需要对应平台 CI，
后两者属于接口明确排除的未定义用法。

对应回归位于 `rendering_correctness`，缓冲区和完整像素接口性能采样分别位于独立的
`image_buffer_performance`、`pixel_access_performance`（`performance` 标签），不会把机器
相关耗时阈值混入功能门禁。
