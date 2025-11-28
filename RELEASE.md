# 版本变更记录

遵循[如何维护更新日志](https://keepachangelog.com/zh-CN/1.0.0/)编写。

## 25.11

### 新增功能

- 增加相机捕获功能，基于 [ccap](https://github.com/wysaid/CameraCapture) 提供相机驱动，新增 `ege::Camera` 类，支持在 C++17 及以上的编译器中使用。
- 增加鼠标双击检测支持，新增 `mouse_msg::is_doubleclick()` 方法。
- 增加鼠标扩展键的检测（如 XBUTTON1, XBUTTON2）。
- 增加 `ege::measuretext` 函数用于测量 `ege_` 前缀函数绘制的文本宽高。
- 增加 `ege::image_convertcolor` 函数用于转换图像像素的颜色类型。
- 增加快速检测按键动作的函数 `keypress`、`keyrelease`、`keyrepeat`。
- 增加 `ege/types.h` 头文件，提供基础类型定义。
- 增加 `kbhit_console` 函数，与 `getch_console` 配套使用。
- 新增 `ege_compress_bound` 函数用于获取压缩数据的最大长度。
- 新增 `color_type` 枚举，用于指定图像像素的颜色类型（`COLORTYPE_PRGB32`, `COLORTYPE_ARGB32`, `COLORTYPE_RGB32`）。

### 改进

- 使用 `stb_image` 替代 `libpng` 解析图像文件，支持更多图像格式（PNG, JPEG, BMP, GIF, TGA, PSD, HDR, PIC, PNM）。
- 使用轻量的 `sdefl`/`sinfl` 库替代 `zlib` 实现数据压缩/解压，移除 `libpng` 和 `zlib` 依赖。
- 图像像素默认颜色格式由 `ARGB32` 改为 `PRGB32`（预乘Alpha），提升渲染效率。
- `color_t` 类型调整为 `uint32_t`，提高代码一致性。
- C++ 标准不低于 C++11 时，颜色枚举 `COLORS` 底层类型设置为 `uint32_t`，与 `color_t` 一致。
- `ege_` 系列文本输出函数的坐标参数类型由 `int` 改为 `float`，支持更精细的定位。
- 图像读取和保存返回更详细的错误信息，增加 `grInvalidFileFormat` 和 `grUnsupportedFormat` 错误码。
- 增加对 Visual Studio 2026 的支持。
- 优化项目配置，在编译器支持 C++17 时自动开启 C++17，并定义宏 `EGE_ENABLE_CPP17=1`。
- 优化静态库编译参数，同时支持 `/MD` 和 `/MT` 编译。
- `ege.h` 提供双语版本（英文版为默认版本，中文版 `ege.zh_CN.h` 主要用于生成文档）。
- 示例代码中的 `sprintf` 改为更安全的 `snprintf`。

### 修复

- 修复垂直方向对齐对 `ege_` 文本绘制函数无效的问题。
- 修复调用 `resizewindow` 后图形出现偏移和被裁剪的问题。
- 修复 `resizewindow` 后视口未进行调整的问题。
- 修复 `ege_fillroundrect()` 单圆角半径参数重载无法在指定图像中绘制的问题。
- 修复使用 `lineto` 前未调整当前点的问题。
- 修复 `bar3d` 图形在线条连接点处有突出，以及在图形堆叠时存在重复边的问题。
- 修复自动渲染模式下长时间无绘制操作时不触发刷新的问题。
- 修复 `IMAGE::gentexture` 会引发栈溢出的问题。
- 修复调用 `delay_ms` 时出现帧率误差较大的问题。
- 修复 `getpixel_f` 和 `putpixels_f` 声明和定义不一致的问题。
- 修复 `MUSIC` 需要依赖 `initgraph()` 初始化的问题。
- 修复部分函数使用 `ege_transform_matrix` 参数做变换时崩溃的问题。
- 修复 `ege/button.h` 中调用 `setfillstyle` 时参数位置错误的问题。
- 纠正 `PIMAGE` 和 `PCIMAGE` 的错误混用。

### 调整

- `alpha_type` 枚举值顺序调整：`ALPHATYPE_PREMULTIPLIED` 现为 0，`ALPHATYPE_STRAIGHT` 为 1。
- `putimage_alphablend` 系列函数的参数由 `alpha_type alphaType` 改为 `color_type colorType`，默认值为 `COLORTYPE_PRGB32`。
- 数据压缩相关函数签名调整，参数类型由 `unsigned long` 改为 `uint32_t`。
- `Bound`、`Rect`、`RectF` 类的方法重命名：`isContains` → `contains`，`isOverlaps` → `overlaps`。
- 修正拼写错误：`tranpose` → `transpose`，`flipHorizonal` → `flipHorizontal`。
- `keystate` 返回值改为 `bool` 类型，参数无效时返回 `false`。
- `kbmsg`、`kbhit` 在运行环境退出后的返回值由 `-1` 改为 `0`，防止阻塞。
- 非阻塞或非延时函数不再触发窗口刷新。
- `flushmouse`、`flushkey` 不再触发窗口刷新。
- 调整初始化环境之前所返回的颜色值，允许预先设置窗口背景色。
- `graphics_errors` 数值改为十进制格式，便于调试。

### 新增示例

- 新增五子棋游戏 Demo（支持简单 AI 对战、落棋音效、抗锯齿棋子）。
- 新增排序可视化 Demo。
- 新增蒙特卡洛法绘制二元函数图像 Demo。
- 新增相机波浪效果 Demo。

### 构建与文档

- 移除 `libpng` 和 `zlib` 依赖，简化编译配置。
- 优化 CMake 配置，添加构建选项允许设置不构建示例程序。
- 优化 GitHub Actions 工作流，增加 MinGW Windows 构建。
- 增加编译测试模块，用于发现编译兼容性问题。
- 添加单元测试相关模块和性能测试逻辑。
- 优化发布脚本，支持跨平台（macOS/Linux/Windows）。
- 文档更新：修正多处文档错误，更新示例代码。

## 24.04

### 更新

- 增加 `ege::putimage_rotatetransparent` 方法。
- 增加 `ege::ege_` 系列绘图函数对 `ege::setlinestyle` 的支持。
- 增加 `ege::ege_drawimage` 和 `ege::ege_transform` 系列函数。
- 增加绘制圆角矩形的函数。
- 增加主控台系列函数。
- 现在 `ege::saveimage` 根据后缀名决定将文件保存为 png 还是 bmp 格式。
- 增加 `ege::setcodepage` 控制 EGE 如何解析程序中的 `char*` 字符串。
- 增加 `ege::setunicodecharmessage` 控制字符消息的编码。

### 修复

- 修复 `ege::inputbox_getline` 界面发黑的问题。
- 修正当半径小于等于 20 时，`ege::sector` 函数绘制错误。
- 修正 `ege::setinitmode` 无法改变窗口位置的问题。
- 使 `sys_edit` 的 `isfocus` 方法可用。
- 修复 Windows 10 下创建窗口时白屏的问题。
- 修复在执行 `ege::outputbox_getline` 后 `ege::outtextxy` 有概率无效的问题。

### 变更

- `ege::getimage` 系列函数现在通过返回值表示是否成功。
- 改用支持 GPU 加速的 `AlphaBlend` 函数实现 `ege::putimage_alpha`。
- 将 `ege::resize` 行为改回会填充背景色，并增加不填充背景色的 `ege::resize_f` 函数。
- 按照 CSS 颜色表修改并增加了预定义颜色值定义。
- `INIT_UNICODE` 初始化选项，改为设置 `setunicodecharmessage(true)`，现在 EGE 总是创建 Unicode 窗口。

## [20.08] - 2020-08-31

### 更新

- 库文件名统一为 `graphics[64].lib` 或 `libgraphics[64].a`。
- 初始化图形对象或调用 `resize` 时，支持将图形对象的长或宽设置为 0。
- 增加 `INIT_UNICODE` 初始化选项，此选项会创建 Unicode 窗口。
- 增加 `ege::seticon` 函数，可通过资源 ID 设置窗口图标。
- 增加 `ege::ege_drawtext`，支持绘制文字时使用 Alpha 通道混合，呈现半透明效果。
- 增加 `putpixel_withalpha` 和 `putpixel_withalpha_f` 函数，支持带透明通道绘制像素点。
- 允许在 `initgraph` 前调用 `newimage` 创建图形对象。
- 支持加载资源中 PNG 格式图片。
- 使 `getkey` 可返回 `key_msg_char` 类型的消息，现在 EGE 支持读取输入法输入了。
- 允许在调用 `initgraph` 前设置窗口标题和图标。
- 使用 CMake 作为编译系统。
- 增加之前缺少的键码。
- 使用 `PCIMAGE` 作为 `const IMAGE*` 的别名，并作为某些函数的形参类型。

### 修复

- 修正 `putimage` 系列函数裁剪区计算错误的 BUG。
- 修复了 `initgraph` 的内存泄漏情况。
- 修复了 `setactivepage()` 和 `setvisualpage()` 无法使用的问题。
- 修正 `putpixel` 等函数颜色格式错误。
- 修正关于线型设置的 BUG。
- 修正某些函数传入 `NULL` 时段错误的 BUG。

### 变更

- 从 `ege.h` 中移出 `EgeControlBase` 的定义到 `ege/egecontrolbase.h` 中。
- 默认字体设置为宋体。
- 生成的静态库文件中不再包含 gdiplus 静态库。
- 改用误差更小的 Alpha 通道混合算法。
- `resize` 不再用默认背景色清空图像。（**在 21.09 中重置**）
- 修改了 `initgraph` 的接口定义。
