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

## 同步规则

GPU 图像第一次只读访问会提交待处理绘制并回读。只要 GPU 内容没有再次变化，后续
`getpixel`、const `getbuffer` 和 `IMAGE_BUFFER_READ` 复用 CPU 缓存，不会按调用次数重复回读。

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

## 指针与线程边界

- 指针在图像 `resize`、重新加载为不同尺寸、删除，或窗口页缓冲重建后失效；之后必须重新获取。
- 图像访问和 OpenGL 上下文仍要求在图形线程串行执行，不支持并发读写同一图像。
- 当前实现每次采样上传整张 CPU Bitmap，没有脏矩形追踪。这优先保证旧裸指针语义；未来可增加
  可选的显式脏区接口作为优化，但默认行为不能依赖用户通知。
- macOS/Linux 目前没有完整的 CPU 绘图后端，因此显式 GPU→CPU Bitmap 转换返回
  `grInvalidMode`，兼容可写 `getbuffer` 仍使用既有 OpenGL 暂存缓冲语义。只读访问和 GPU 缓存
  行为在各平台一致。

## 自动化覆盖边界

Windows 回归同时运行 GDI 和 OpenGL 模式，覆盖访问意图、显式转换、普通及 GDI+ 增强状态
（仿射变换和渐变画刷）与 resize 迁移、
保留指针跨 EGE 绘制继续写入、可视页上屏，以及 CPU Bitmap→GPU 和 GPU→CPU Bitmap 两个
方向。贴图桥覆盖基础/拉伸复制、透明色、Alpha/Alpha-transparent/with-alpha、alpha filter、
旋转、增强贴图、纹理填充和 `getimage`；文件解码还验证内部写入不会误触发公共可写缓冲转换。

无法在 Windows 机器上自动证明的边界是 macOS/Linux 的运行时行为、第三方代码在图形线程外
并发写裸指针，以及用户在 `IMAGE_BUFFER_READ` 承诺只读后仍写入。前者需要对应平台 CI，
后两者属于接口明确排除的未定义用法。

对应回归位于 `rendering_correctness`，性能采样位于独立的
`image_buffer_performance`（`performance` 标签），不会把机器相关耗时阈值混入功能门禁。
