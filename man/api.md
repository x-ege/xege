# EGE (Easy Graphics Engine) API 参考文档

## 简介

EGE（Easy Graphics Engine）是一个适用于初学者的图形库，具有类似于 `graphics.h` 的 BGI 图形库接口，使用方法接近 TC 图形编程。EGE 为学习和教学提供了简单易用的图形编程环境。

### 主要特性

- **兼容性强**：支持 Visual Studio 6.0 至 Visual Studio 2022，MinGW，小熊猫C++等多种开发环境
- **功能丰富**：提供基础绘图、高级绘图（抗锯齿、透明度）、图像处理、动画编程等功能
- **多媒体支持**：支持多种图像格式（BMP, JPG, PNG, GIF）和音频播放
- **跨平台**：支持 Windows 原生开发，以及 Linux/macOS 下的交叉编译

## API 模块概览

EGE 库的 API 按功能分为以下主要模块：

| 模块 | 功能描述 |
|------|----------|
| [绘图环境 (env)](#绘图环境-env) | 窗口创建、画布管理、坐标系统 |
| [颜色 (col)](#颜色-col) | 颜色定义、颜色空间转换 |
| [图形绘制 (draw)](#图形绘制-draw) | 基础图形绘制、高级绘图功能 |
| [文字输出 (font)](#文字输出-font) | 文本显示、字体设置 |
| [图像处理 (img)](#图像处理-img) | 图片加载、显示、处理 |
| [键盘鼠标输入 (input)](#键盘鼠标输入-input) | 用户交互事件处理 |
| [时间 (time)](#时间-time) | 时间相关函数、延时控制 |
| [控制台 (console)](#控制台-console) | 控制台输入输出 |
| [数学 (math)](#数学-math) | 数学计算辅助函数 |
| [随机数 (rand)](#随机数-rand) | 随机数生成 |
| [音乐 (music)](#音乐-music) | 音频播放功能 |
| [预定义常量 (macro)](#预定义常量-macro) | 预定义的常量和宏 |
| [其它 (other)](#其它-other) | 其他辅助功能 |

---

## 绘图环境 (env)

绘图环境模块提供窗口创建、画布管理和坐标系统设置等基础功能。

### 核心函数

#### 窗口初始化

- `initgraph(int width, int height, int flag = 0)` - 初始化图形窗口
- `closegraph()` - 关闭图形窗口
- `getwidth()` - 获取窗口宽度
- `getheight()` - 获取窗口高度

#### 画布操作

- `setworkingimage(PIMAGE pimg = NULL)` - 设置当前工作画布
- `getworkingimage()` - 获取当前工作画布
- `cleardevice(PIMAGE pimg = NULL)` - 清空画布

#### 窗口设置

- `setinitmode(int mode, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT)` - 设置窗口初始化模式
- `setcaption(LPCSTR caption)` - 设置窗口标题
- `movewindow(int x, int y, bool redraw = true)` - 移动窗口位置
- `resizewindow(int width, int height)` - 调整窗口大小

#### 渲染控制

- `delay_fps(int fps)` - 帧率控制
- `delay_ms(int ms)` - 毫秒延时
- `delay_jfps(int fps)` - 精确帧率控制

---

## 颜色 (col)

颜色模块提供颜色定义、RGB/HSL/HSV颜色空间转换等功能。

### 颜色类型

- `color_t` - 颜色类型定义
- `COLORREF` - Windows 颜色引用类型

### 颜色创建

- `rgb(int r, int g, int b)` - 创建 RGB 颜色
- `rgba(int r, int g, int b, int a)` - 创建 RGBA 颜色（含透明度）
- `hsl(float h, float s, float l)` - 创建 HSL 颜色
- `hsv(float h, float s, float v)` - 创建 HSV 颜色

### 颜色转换

- `rgb2hsl(color_t rgb, float* h, float* s, float* l)` - RGB 到 HSL 转换
- `rgb2hsv(color_t rgb, float* h, float* s, float* v)` - RGB 到 HSV 转换
- `hsl2rgb(float h, float s, float l)` - HSL 到 RGB 转换
- `hsv2rgb(float h, float s, float v)` - HSV 到 RGB 转换

### 颜色操作

- `getr(color_t color)` - 获取红色分量
- `getg(color_t color)` - 获取绿色分量
- `getb(color_t color)` - 获取蓝色分量
- `geta(color_t color)` - 获取透明度分量

---

## 图形绘制 (draw)

图形绘制模块提供基础图形绘制和高级绘图功能。

### 基础绘图

- `putpixel(int x, int y, color_t color, PIMAGE pimg = NULL)` - 画点
- `getpixel(int x, int y, PIMAGE pimg = NULL)` - 获取点颜色
- `line(int x1, int y1, int x2, int y2, PIMAGE pimg = NULL)` - 画线
- `rectangle(int left, int top, int right, int bottom, PIMAGE pimg = NULL)` - 画矩形
- `circle(int x, int y, int radius, PIMAGE pimg = NULL)` - 画圆
- `ellipse(int x, int y, int xradius, int yradius, PIMAGE pimg = NULL)` - 画椭圆

### 填充绘图

- `bar(int left, int top, int right, int bottom, PIMAGE pimg = NULL)` - 填充矩形
- `fillcircle(int x, int y, int radius, PIMAGE pimg = NULL)` - 填充圆
- `fillellipse(int x, int y, int xradius, int yradius, PIMAGE pimg = NULL)` - 填充椭圆
- `floodfill(int x, int y, color_t border, PIMAGE pimg = NULL)` - 漫水填充

### 高级绘图

- `ege_line(float x1, float y1, float x2, float y2, PIMAGE pimg = NULL)` - 高精度线条（支持抗锯齿）
- `ege_circle(float x, float y, float radius, PIMAGE pimg = NULL)` - 高精度圆（支持抗锯齿）
- `ege_ellipse(float x, float y, float xradius, float yradius, PIMAGE pimg = NULL)` - 高精度椭圆
- `ege_rectangle(float x, float y, float w, float h, PIMAGE pimg = NULL)` - 高精度矩形

### 绘图设置

- `setcolor(color_t color, PIMAGE pimg = NULL)` - 设置绘图颜色
- `setfillcolor(color_t color, PIMAGE pimg = NULL)` - 设置填充颜色
- `setlinewidth(float width, PIMAGE pimg = NULL)` - 设置线宽
- `setlinestyle(int style, int thickness = 1, PIMAGE pimg = NULL)` - 设置线型

---

## 文字输出 (font)

文字输出模块提供文本显示和字体设置功能。

### 文本输出

- `outtextxy(int x, int y, LPCSTR textstring, PIMAGE pimg = NULL)` - 在指定位置输出文本
- `outtext(LPCSTR textstring, PIMAGE pimg = NULL)` - 在当前位置输出文本
- `xyprintf(int x, int y, LPCSTR fmt, ...)` - 格式化文本输出

### 字体设置

- `settextfont(int height, int width, LPCSTR fontname, PIMAGE pimg = NULL)` - 设置文本字体
- `settextstyle(int height, int width, LPCSTR fontname, int orientation = 0, int weight = 0, bool italic = false, bool underline = false, bool strikeout = false, PIMAGE pimg = NULL)` - 设置文本样式

### 文本测量

- `textwidth(LPCSTR textstring, PIMAGE pimg = NULL)` - 获取文本宽度
- `textheight(LPCSTR textstring, PIMAGE pimg = NULL)` - 获取文本高度

---

## 图像处理 (img)

图像处理模块提供图片加载、显示、处理和保存功能。

### 图像创建与管理

- `newimage(PIMAGE pimg, int width, int height)` - 创建新图像
- `delimage(PIMAGE pimg)` - 删除图像
- `getimage(PIMAGE pimg, int srcx, int srcy, int srcw, int srch)` - 获取屏幕图像

### 图像加载与保存

- `getimage(PIMAGE pimg, LPCSTR imgfile)` - 从文件加载图像
- `saveimage(PIMAGE pimg, LPCSTR filename)` - 保存图像到文件

### 图像显示

- `putimage(int dstx, int dsty, PIMAGE pimg, DWORD dwRop = SRCCOPY)` - 显示图像
- `putimage(int dstx, int dsty, int dstw, int dsth, PIMAGE pimg, int srcx, int srcy, DWORD dwRop = SRCCOPY)` - 缩放显示图像
- `putimage_transparent(PIMAGE imgdest, PIMAGE imgsrc, int nXOriginDest, int nYOriginDest, color_t crTransparent, int nXOriginSrc = 0, int nYOriginSrc = 0, int nWidthSrc = 0, int nHeightSrc = 0)` - 透明显示图像

### 图像属性

- `getwidth(PIMAGE pimg)` - 获取图像宽度
- `getheight(PIMAGE pimg)` - 获取图像高度

---

## 键盘鼠标输入 (input)

输入模块提供键盘和鼠标事件处理功能。

### 键盘输入

- `kbhit()` - 检测键盘输入
- `getch()` - 获取字符输入（不回显）
- `getche()` - 获取字符输入（回显）

### 鼠标输入

- `mouse_msg` - 鼠标消息结构体
- `getmouse()` - 获取鼠标消息
- `mousepos(int* x, int* y)` - 获取鼠标位置

### 事件处理

- `getkey()` - 获取按键消息
- `keystate(int key)` - 获取按键状态

---

## 时间 (time)

时间模块提供时间获取和延时控制功能。

### 时间获取

- `clock()` - 获取程序运行时间
- `time(time_t* t)` - 获取当前时间
- `fclock()` - 获取高精度时间

### 延时函数

- `delay(int ms)` - 毫秒延时
- `sleep(int ms)` - 毫秒睡眠

---

## 控制台 (console)

控制台模块提供控制台输入输出功能。

### 控制台输出

- `printf(const char* format, ...)` - 格式化输出
- `puts(const char* str)` - 字符串输出

### 控制台输入

- `scanf(const char* format, ...)` - 格式化输入
- `gets(char* buffer)` - 字符串输入

---

## 数学 (math)

数学模块提供常用数学计算函数。

### 三角函数

- `sin(double x)` - 正弦函数
- `cos(double x)` - 余弦函数
- `tan(double x)` - 正切函数

### 数学运算

- `abs(int x)` - 绝对值
- `sqrt(double x)` - 平方根
- `pow(double x, double y)` - 幂运算

---

## 随机数 (rand)

随机数模块提供随机数生成功能。

### 随机数生成

- `randomize()` - 初始化随机数种子
- `random(int x)` - 生成 0 到 x-1 的随机数
- `rand()` - 生成随机数
- `srand(unsigned int seed)` - 设置随机数种子

---

## 音乐 (music)

音乐模块提供音频播放功能。

### 音频播放

- `ege_enable_music(bool enable)` - 启用/禁用音乐功能
- `music(LPCSTR filename)` - 播放音乐文件

---

## 预定义常量 (macro)

### 颜色常量

- `BLACK`, `BLUE`, `GREEN`, `CYAN`, `RED`, `MAGENTA`, `BROWN`, `LIGHTGRAY`
- `DARKGRAY`, `LIGHTBLUE`, `LIGHTGREEN`, `LIGHTCYAN`, `LIGHTRED`, `LIGHTMAGENTA`, `YELLOW`, `WHITE`

### 按键常量

- `VK_SPACE`, `VK_ENTER`, `VK_ESCAPE`, `VK_UP`, `VK_DOWN`, `VK_LEFT`, `VK_RIGHT`
- `VK_F1` ~ `VK_F12`, `VK_SHIFT`, `VK_CTRL`, `VK_ALT`

### 鼠标常量

- `MOUSEMOVE`, `MOUSEDOWN`, `MOUSEUP`, `MOUSEDRAG`
- `LEFT_BUTTON`, `RIGHT_BUTTON`, `MIDDLE_BUTTON`

### 窗口初始化标志

- `INIT_DEFAULT`, `INIT_NOBORDER`, `INIT_TOPMOST`, `INIT_RENDERMANUAL`

---

## 其它 (other)

### 版本信息

- `EGE_VERSION` - EGE 版本号
- `getversion()` - 获取版本信息

### 调试辅助

- `assert(bool condition)` - 断言
- `getfps()` - 获取当前帧率

---

## 使用示例

### 基础绘图示例

```cpp
#include <graphics.h>

int main() {
    // 初始化图形窗口
    initgraph(640, 480);
    
    // 设置绘图颜色
    setcolor(RED);
    
    // 绘制图形
    circle(320, 240, 100);
    rectangle(100, 100, 540, 380);
    
    // 等待按键
    getch();
    
    // 关闭图形窗口
    closegraph();
    
    return 0;
}
```

### 动画示例

```cpp
#include <graphics.h>

int main() {
    initgraph(640, 480);
    
    int x = 0;
    while (!kbhit()) {
        cleardevice();
        circle(x, 240, 50);
        x = (x + 5) % 640;
        delay_fps(60);
    }
    
    closegraph();
    return 0;
}
```

---

## 注意事项

1. **编译器兼容性**：确保使用支持的编译器版本
2. **库文件链接**：需要正确链接 EGE 库文件
3. **窗口初始化**：在使用任何绘图函数前必须先调用 `initgraph()`
4. **资源释放**：程序结束前调用 `closegraph()` 释放资源
5. **坐标系统**：EGE 使用屏幕坐标系，原点在左上角

---

## 更多资源

- **官方网站**：<https://xege.org>
- **源码仓库**：<https://github.com/wysaid/xege>
- **社区论坛**：<https://club.xege.org>
- **在线文档**：<http://xege.org/manual>

---

*本文档基于 EGE 24.04 版本编写*
