# EGE 测试快速入门指南

## 概述

本次改进为EGE项目添加了完整的测试分类系统，将测试分为**功能性测试**和**性能测试**两大类别，并新增了6个测试文件，覆盖率从原来的3-5%提升到约20.5%。

## 测试分类

### 🧪 功能性测试 (Functional Tests)
验证API的正确性、功能是否符合预期、边界条件处理

### ⚡ 性能测试 (Performance Tests)  
测量API的执行速度、吞吐量、不同参数下的性能表现

## 快速开始

### 1. 构建测试

```bash
cd /path/to/xege
mkdir -p build && cd build

# 构建所有测试
cmake .. -DEGE_BUILD_TEST=ON
cmake --build .
```

### 2. 运行测试

```bash
# 运行所有测试
ctest

# 只运行功能性测试
ctest -L functional

# 只运行性能测试
ctest -L performance

# 运行特定测试
ctest -R drawing_primitives

# 显示详细输出
ctest -V -L functional
```

### 3. 直接运行测试程序

```bash
cd bin

# 功能性测试
./drawing_primitives_test
./color_operations_test
./image_management_test
./window_management_test

# 性能测试
./drawing_performance_test
./pixel_operations_performance_test
```

## 测试文件清单

### 新增功能性测试

| 文件名 | 测试内容 | CTest名称 |
|--------|----------|-----------|
| drawing_primitives_test.cpp | 直线、矩形、圆形、椭圆、弧线、扇形等 | functional_drawing_primitives |
| color_operations_test.cpp | 颜色设置/获取、RGB/HSV/HSL转换 | functional_color_operations |
| image_management_test.cpp | 图像创建/删除/复制/尺寸操作 | functional_image_management |
| window_management_test.cpp | 窗口初始化/标题/可见性/视口 | functional_window_management |

### 新增性能测试

| 文件名 | 测试内容 | CTest名称 |
|--------|----------|-----------|
| drawing_performance_test.cpp | 图形绘制函数的性能基准 | performance_drawing |
| pixel_operations_performance_test.cpp | 像素操作的性能基准 | performance_pixel_operations |

### 已有测试（重新分类）

**功能性测试:**
- putimage_basic_test
- putimage_transparent_test  
- putimage_rotate_test
- putimage_comparison_test
- putimage_alphablend_comprehensive_test

**性能测试:**
- putimage_performance_test
- putimage_alphablend_test

## 构建选项

```bash
# 只构建功能性测试
cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_FUNCTIONAL=ON -DEGE_TEST_PERFORMANCE=OFF

# 只构建性能测试
cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_FUNCTIONAL=OFF -DEGE_TEST_PERFORMANCE=ON

# 构建所有测试（默认）
cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_FUNCTIONAL=ON -DEGE_TEST_PERFORMANCE=ON

# 不构建测试
cmake .. -DEGE_BUILD_TEST=OFF
```

## 测试覆盖率统计

### 当前覆盖情况

| API类别 | 总函数数 | 已测试 | 覆盖率 |
|---------|----------|--------|--------|
| 图像操作 (putimage系列) | 20 | 12 | 60% |
| 图形绘制基础 | 60 | 24 | 40% |
| 颜色操作 | 30 | 15 | 50% |
| 图像管理 | 15 | 7 | 47% |
| 窗口管理 | 30 | 11 | 37% |
| 像素操作 | 20 | 6 | 30% |
| **总计** | **365** | **75** | **20.5%** |

### 未覆盖的API类别（优先级排序）

1. **文本渲染** (0%) - outtextxy, settextstyle, etc.
2. **输入处理** (0%) - 鼠标、键盘输入
3. **变换矩阵** (0%) - 图形变换操作
4. **高级图形** (0%) - Bezier曲线、填充等
5. **音乐播放** (0%) - Music类
6. **控件系统** (0%) - Button, Label等控件
7. **相机捕获** (0%) - camera_capture类

## 测试示例

### 功能性测试示例

```cpp
// drawing_primitives_test.cpp 中的一个测试
bool testLineDrawing() {
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试水平线
    setcolor(RED);
    line(10, 50, 190, 50, img);
    
    // 验证线上的点
    bool horizontalLineOk = verifyPixelColor(img, 100, 50, RED, 10);
    
    delimage(img);
    return horizontalLineOk;
}
```

### 性能测试示例

```cpp
// drawing_performance_test.cpp 中的一个测试
void testLinePerformance() {
    PIMAGE img = newimage(800, 600);
    
    // 运行1000次迭代
    BatchPerformanceTest test("Horizontal line", 1000);
    test.runBatch([&]() {
        line(100, 300, 700, 300, img);
    }, 1000);
    
    // 输出结果：平均时间、操作/秒、最小/最大时间
    printPerformanceResult("Horizontal line", test);
    
    delimage(img);
}
```

## 持续集成建议

如果要集成到CI/CD流程，建议：

```bash
# 在CI中运行功能性测试
ctest -L functional --output-on-failure

# 在nightly build中运行性能测试
ctest -L performance --output-on-failure

# 生成测试报告
ctest --output-junit test_results.xml
```

## 故障排除

### 问题：编译器找不到
```
解决方案：确保已安装MinGW或Visual Studio，并配置好环境变量
```

### 问题：测试运行时窗口一闪而过
```
解决方案：测试已经设置了SHOW_CONSOLE=1，并且使用hidewindow()隐藏图形窗口
```

### 问题：测试失败
```
解决方案：
1. 查看详细输出: ctest -V -R <test_name>
2. 直接运行测试程序查看错误信息
3. 检查是否在Windows环境或Wine环境下运行
```

## 参考文档

- **TEST_COVERAGE.md** - 详细的覆盖率分析和未测试API清单
- **README.md** - 完整的测试套件说明
- **各测试源文件** - 包含具体的测试实现和注释

## 贡献

要添加新的测试：

1. 在 `tests/tests/` 目录创建新的测试文件
2. 在 `tests/CMakeLists.txt` 中注册测试
3. 为测试添加合适的标签 (functional 或 performance)
4. 更新 TEST_COVERAGE.md 中的覆盖率统计
5. 提交PR并说明测试内容

---

**版本**: 1.0  
**最后更新**: 2025-10-31
