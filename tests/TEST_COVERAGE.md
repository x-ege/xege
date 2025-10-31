# EGE 单元测试覆盖率报告

## 测试分类

本测试套件将测试分为两大类：

### 1. 功能性测试 (Functional Tests)
功能性测试验证API的正确性、边界条件和错误处理。

### 2. 性能测试 (Performance Tests)
性能测试测量API的执行速度、吞吐量和资源使用效率。

## 运行测试

### 运行所有测试
```bash
cd build
ctest
```

### 只运行功能性测试
```bash
ctest -L functional
```

### 只运行性能测试
```bash
ctest -L performance
```

### 运行特定测试
```bash
ctest -R <test_name>
# 例如:
ctest -R functional_drawing_primitives
ctest -R performance_putimage
```

## 构建选项

可以通过CMake选项控制构建哪些测试：

```bash
# 只构建功能性测试
cmake .. -DEGE_TEST_PERFORMANCE=OFF -DEGE_TEST_FUNCTIONAL=ON

# 只构建性能测试
cmake .. -DEGE_TEST_PERFORMANCE=ON -DEGE_TEST_FUNCTIONAL=OFF

# 构建所有测试 (默认)
cmake .. -DEGE_TEST_PERFORMANCE=ON -DEGE_TEST_FUNCTIONAL=ON
```

## 当前测试覆盖率

### API总数
根据 `include/ege.h` 分析，EGE库包含约 **365个公开API函数**。

### 已测试的API类别

#### 1. 图像操作 (putimage系列) - 覆盖率: ~60%

**功能性测试:**
- ✅ putimage_basic_test - 基础putimage操作
- ✅ putimage_transparent_test - 透明色处理
- ✅ putimage_rotate_test - 图像旋转
- ✅ putimage_comparison_test - 对比测试
- ✅ putimage_alphablend_comprehensive_test - Alpha混合综合测试

**性能测试:**
- ✅ putimage_performance_test - putimage性能基准
- ✅ putimage_alphablend_test - Alpha混合性能

**覆盖的API:**
- putimage (6个重载)
- putimage_transparent
- putimage_alphablend (4个重载)
- putimage_withalpha (2个重载)
- putimage_rotate
- putimage_rotatezoom
- putimage_rotatetransparent (2个重载)
- putimage_alphatransparent
- putimage_alphafilter

**未覆盖的API:**
- 部分边界条件和错误处理

#### 2. 图形绘制基础 - 覆盖率: ~40% (新增)

**功能性测试:**
- ✅ drawing_primitives_test - 绘图基础函数

**性能测试:**
- ✅ drawing_performance_test - 绘图性能基准

**覆盖的API:**
- line, line_f
- rectangle, rectangle_f
- circle, circle_f
- ellipse, ellipsef
- arc, arcf
- fillrectangle, fillrectangle_f
- fillcircle, fillcircle_f
- fillellipse, fillellipsef
- bar, bar3d
- pie, pief, fillpie, fillpief
- pieslice, pieslicef

**未覆盖的API:**
- lineto, lineto_f, linerel, linerel_f
- solidpie, solidpief
- sector, sectorf
- drawpoly, fillpoly, drawlines

#### 3. 颜色操作 - 覆盖率: ~50% (新增)

**功能性测试:**
- ✅ color_operations_test - 颜色操作测试

**覆盖的API:**
- setcolor, getcolor
- setlinecolor, getlinecolor
- setfillcolor, getfillcolor
- setbkcolor, getbkcolor
- settextcolor
- setfontbkcolor
- RGB, RGBTOBGR, EGERGB, EGERGBA, EGEARGB, EGEACOLOR
- EGEGET_R, EGEGET_G, EGEGET_B, EGEGET_A
- hsv2rgb, rgb2hsv, hsl2rgb, rgb2hsl

**未覆盖的API:**
- setbkcolor_f
- setbkmode
- 颜色空间转换的边界情况

#### 4. 图像管理 - 覆盖率: ~45% (新增)

**功能性测试:**
- ✅ image_management_test - 图像生命周期管理

**覆盖的API:**
- newimage, delimage
- getimage (4个重载)
- saveimage
- getwidth, getheight
- getx, gety
- resize

**未覆盖的API:**
- savepng
- savejpg
- getimage_pngfile

#### 5. 窗口管理 - 覆盖率: ~35% (新增)

**功能性测试:**
- ✅ window_management_test - 窗口管理测试

**覆盖的API:**
- initgraph, closegraph
- is_run
- setcaption
- showwindow, hidewindow
- movewindow, resizewindow
- setviewport, getviewport
- cleardevice
- settarget

**未覆盖的API:**
- setinitmode, getinitmode
- seticon
- attachHWND
- flushwindow
- setrendermode
- setactivepage, setvisualpage
- window_setviewport, window_getviewport

#### 6. 像素操作 - 覆盖率: ~30% (新增)

**性能测试:**
- ✅ pixel_operations_performance_test - 像素操作性能

**覆盖的API:**
- getpixel, getpixel_f
- putpixel, putpixel_f
- putpixel_withalpha, putpixel_withalpha_f
- putpixel_alphablend, putpixel_alphablend_f

**未覆盖的API:**
- putpixels, putpixels_f
- putpixel_savealpha, putpixel_savealpha_f
- getpixels

### 未测试的API类别

#### 7. 文本渲染 - 覆盖率: 0%
- outtextxy, outtextxy_f
- outtext
- settextstyle
- setfont
- getfont
- textheight, textwidth
- xyprintf
- drawtext

#### 8. 输入处理 - 覆盖率: 0%
- getmouse
- keystate, kbhit, getch
- kbmsg, kbhit_console, getch_console
- getkey
- showmouse, mousemsg

#### 9. 音乐播放 - 覆盖率: 0%
- music类的所有方法

#### 10. 变换矩阵 - 覆盖率: 0%
- ege_transform_matrix相关API
- ege_set_transform, ege_get_transform
- ege_transform_rotate, ege_transform_translate, etc.

#### 11. 高级图形 - 覆盖率: 0%
- ege_drawtext
- ege_drawimage
- Bezier曲线
- floodfill, floodfillsurface

#### 12. 控件系统 - 覆盖率: 0%
- egeControlBase类
- Button, Label等控件

#### 13. 相机捕获 - 覆盖率: 0%
- camera_capture类 (如果启用C++17)

#### 14. 其他工具函数 - 覆盖率: 0%
- delay_fps, delay_ms, delay_jfps
- getfps, getHWnd, getdc
- api_sleep, fclock, 等

## 总体覆盖率估算

| 类别 | 函数数量(估算) | 已测试 | 覆盖率 |
|------|--------------|--------|--------|
| 图像操作 (putimage系列) | 20 | 12 | 60% |
| 图形绘制基础 | 60 | 24 | 40% |
| 颜色操作 | 30 | 15 | 50% |
| 图像管理 | 15 | 7 | 47% |
| 窗口管理 | 30 | 11 | 37% |
| 像素操作 | 20 | 6 | 30% |
| 文本渲染 | 25 | 0 | 0% |
| 输入处理 | 30 | 0 | 0% |
| 音乐播放 | 20 | 0 | 0% |
| 变换矩阵 | 15 | 0 | 0% |
| 高级图形 | 30 | 0 | 0% |
| 控件系统 | 40 | 0 | 0% |
| 相机捕获 | 10 | 0 | 0% |
| 其他工具 | 20 | 0 | 0% |
| **总计** | **365** | **75** | **20.5%** |

## 改进前后对比

### 改进前
- 测试文件: 7个
- 覆盖的API类别: 1个 (putimage系列)
- 估算覆盖率: ~3-5%
- 测试分类: 无明确分类

### 改进后
- 测试文件: 13个
- 覆盖的API类别: 6个
- 估算覆盖率: ~20.5%
- 测试分类: 明确区分性能测试和功能性测试

## 下一步改进建议

### 短期目标 (覆盖率提升至40%)
1. **文本渲染测试** - 添加outtextxy, setfont等测试
2. **输入处理测试** - 添加模拟输入的测试
3. **填充和图案测试** - 补充floodfill等测试

### 中期目标 (覆盖率提升至60%)
4. **变换矩阵测试** - 完整的transform API测试
5. **高级图形测试** - Bezier曲线等
6. **音乐播放测试** - Music类测试

### 长期目标 (覆盖率提升至80%+)
7. **控件系统测试** - 所有控件的测试
8. **相机捕获测试** - 如果环境支持
9. **边界条件和错误处理** - 全面的负面测试
10. **内存泄漏检测** - 使用工具检测内存问题

## 测试指标

### 功能性测试指标
- ✅ API调用成功率
- ✅ 输出正确性验证
- ✅ 边界条件处理
- ✅ 错误处理验证
- ✅ 内存管理正确性

### 性能测试指标
- ✅ 平均执行时间 (ms)
- ✅ 操作吞吐量 (ops/sec)
- ✅ 最小/最大执行时间
- ✅ 标准差 (性能稳定性)
- ✅ FPS (帧率)
- ✅ 不同分辨率下的性能表现

## 测试环境要求

- Windows操作系统 (或Wine模拟环境)
- MinGW或Visual Studio编译器
- CMake 3.13或更高版本
- 足够的显存和内存 (特别是高分辨率测试)

## 贡献指南

要添加新的测试用例:

1. 确定测试类型 (功能性 vs 性能)
2. 在`tests/tests/`目录创建新的.cpp文件
3. 使用TestFramework和PerformanceTimer
4. 在CMakeLists.txt中注册测试
5. 更新本文档的覆盖率信息
6. 确保测试可以独立运行

## 测试框架工具

### TestFramework
- 窗口管理和初始化
- 测试结果收集
- 日志和报告功能

### PerformanceTimer
- 高精度计时
- 批量性能测试
- 统计分析 (平均值、标准差等)

### ImageGenerator
- 生成各种测试图案
- 支持多种分辨率
- 渐变、纹理、噪点等图案

## 附录: 测试命令速查

```bash
# 构建所有测试
cmake --build . --target all

# 运行所有测试
ctest

# 详细输出
ctest -V

# 只运行失败的测试
ctest --rerun-failed

# 并行运行测试
ctest -j4

# 生成测试报告
ctest --output-on-failure

# 按标签筛选
ctest -L functional      # 只运行功能性测试
ctest -L performance     # 只运行性能测试
```

---

**最后更新:** 2025-10-31
**版本:** 1.0
