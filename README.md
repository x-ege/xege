# EGE (Easy Graphics Engine)

[![MSVC Build](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/msvc-build.yml)
[![MinGW Build](https://github.com/x-ege/xege/actions/workflows/mingw-build.yml/badge.svg)](https://github.com/x-ege/xege/actions/workflows/mingw-build.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://github.com/x-ege/xege)

![logo](egelogo.jpg)

- Website <https://xege.org>  (官网, 附带[教程](https://xege.org/beginner-lesson-1.html)以及[范例](https://xege.org/category/demo)等)
- HomePage <https://github.com/wysaid/xege>  (代为维护)
- 社区 [ege club](https://club.xege.org)
- 百度贴吧 [ege](http://tieba.baidu.com/f?kw=ege)
- 百度贴吧 [ege娘](http://tieba.baidu.com/f?kw=ege%C4%EF)
- 教程以及介绍 [EGE教程&介绍](https://blog.csdn.net/qq_39151563/article/details/100154767) (by [`依稀`](https://blog.csdn.net/qq_39151563?type=blog) )
- E-Mail new: <this@xege.org>
- QQ Group: 1060223135 (正常, 可加入)

>原作者 misakamm 联系方式与相关链接：
>
> - ~~HomePage1 <http://misakamm.github.com/xege>(原作者仓库, 已停止更新)~~
> - ~~HomePage2 <http://misakamm.bitbucket.org/index.htm>(同上)~~
> - ~~google+ <https://plus.google.com/communities/103680008540979677071>(无法访问)~~
> - ~~Blog: <https://misakamm.com>(无法访问)~~
> - ~~E-Mail: <misakamm@gmail.com>(无法访问)~~
>
> 详细帮助文档，在压缩包里的man目录下，用浏览器打开`index.htm`就可以看到了
> 原作者: [misakamm](https://github.com/misakamm/xege), 目前下落不明
> 暂由 <https://github.com/wysaid/xege> 代为更新

## EGE图形库

　　`EGE (Easy Graphics Engine)` 是 Windows 下的简易绘图库，是一个类似 BGI(graphics.h) 的面向 C/C++ 语言新手的图形库，它的目标也是为了替代 TC 的 BGI 库而存在。它的使用方法与 TC 中的 `graphics.h` 相当接近，对新手来说，简单，友好，容易上手，免费开源，而且因为接口意义直观，即使是之前完全没有接触过图形编程的，也能迅速学会基本的绘图。 目前，EGE 图形库已经完美支持 Visual C++ 6.0, Visual Studio 2008 ~ Visual Studio 2022, 小熊猫C++, CLion, C-Free, Dev-C++, Code::Blocks, wxDev, Eclipse for C/C++ 等 IDE，即支持使用 MinGW 为编译环境的IDE。如果你需要在 VC 下使用 `graphics.h`，那么 ege 将会是很好的替代品。

### API 文档

可参考: [API 文档](man/api.md)

## 为什么要写这个库？

### 许多学编程的都是从 C 语言开始入门的，而目前的现状是

1. 有些学校以 Turbo C 作为 C 语言编程环境教学, 只是 Turbo C 的环境实在太老了，复制粘贴都很不方便。并且 DOS 环境在现在的操作系统支持很有限，并且 DOS 下可用颜色数太少。

2. 有些学校直接拿 VC 来讲 C 语言，因为 VC 的编辑和调试环境都很优秀，并且 VC 有适合教学的免费版本。可惜初学者在 VC 下一般只会做一些文字性的练习题，想画条直线画个圆都很难，还要注册窗口类、建消息循环等等，初学者会受严重打击的，甚至有初学者以为 C 只能在“黑框”下使用。

3. 还有计算机图形学，这门课程的重点是绘图算法，而不是 Windows 编程。所以，许多老师不得不用 TC 教学，因为 Windows 绘图太复杂了，会偏离教学的重点。新的图形学的书有不少是用的 OpenGL，可是门槛依然很高。
　　如果您刚开始学 C 语言，或者您是一位教C语言的老师，再或者您在教计算机图形学，那么这个库一定会让您兴奋的。采用 ege 图形库，您将可以在 VC 的环境中方便的处理和生成图像，甚至制作动画和游戏。

### ege图形库的优点

- 效率较好　特别在窗口锁定绘图模式下，640 x 480 的半透明混合，可以直接使用 `getpixel` / `putpixel` 完成，并且优化后可以在大约 1.5 GHz CPU台式机器上达到 60 FPS （60帧/秒）
- 灵活性强　绘图可以直接针对一个图像，或者画在控件上，不必只能画在屏幕上
- 功能更多　支持拉伸贴图，支持图片旋转，支持透明半透明贴图，支持图像模糊滤镜操作，可以用对话框函数进行图形化的输入，可以方便地对帧率进行准确的控制，可以读取常见的流行的图片格式（bmp/jpg/png），可以保存图片为 bmp 或 png 格式。
- 免费开源　本图形库为免费开源的图形库，你不但可以获取本图形库的全部源代码，你也可以参与到本图形库的开发，详情请联系 wysaid (<this@xege.org>)。

## ege简要使用说明

　　目前模拟了绝大多数 BGI 的绘图函数。使用上，基本的绘图函数和 TC / BC 没太大区别。看一个画圆的例子吧：

```cpp
#include <graphics.h> // 就是需要引用这个图形库

int main()
{
    initgraph(640, 480); // 初始化，显示一个窗口，这里和 TC 略有区别
    circle(200, 200, 100); // 画圆，圆心(200, 200)，半径 100
    getch(); // 暂停一下等待用户按键
    closegraph(); // 关闭图形界面
    return 0;
}
```

> 呵呵，很简单吧。更详细的请参阅 ege 文档。

EGE 使用 [CMake](https://cmake.org) 构建编译系统，可参阅 [编译指南](BUILD.md) 自行编译。
