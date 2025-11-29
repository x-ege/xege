# EGE Test Suite (EGE 测试套件)

这是EGE图形库的完整测试套件，包含功能性测试和性能测试。

## 测试分类

### 🧪 功能性测试 (Functional Tests)
验证API的正确性、边界条件和错误处理。

### ⚡ 性能测试 (Performance Tests)
测量API的执行速度、吞吐量和资源使用效率。

## 功能特性

- 🚀 **完整的测试覆盖** - 涵盖核心图形API
- 📊 **多分辨率测试** - 从64x64到8K分辨率的全面测试
- ⏱️ **精确的性能测量** - 使用高精度计时器和统计分析
- 🎯 **自动化测试框架** - 无需手动干预的批量测试
- 💻 **控制台输出保持** - 定义SHOW_CONSOLE=1保持控制台窗口可见
- 🖼️ **智能图像生成** - 多种测试图案自动生成
- 🏷️ **测试标签分类** - 支持按类别运行测试

## 测试内容

### 功能性测试 (Functional Tests)

#### 1. 图像操作测试
- **putimage_basic_test** - 基础putimage功能测试
- **putimage_transparent_test** - 透明色处理测试
- **putimage_rotate_test** - 图像旋转测试
- **putimage_comparison_test** - 图像对比测试
- **putimage_alphablend_comprehensive_test** - Alpha混合综合测试

#### 2. 图形绘制测试
- **drawing_primitives_test** - 基础图形绘制(线、圆、矩形等)

#### 3. 颜色操作测试
- **color_operations_test** - 颜色设置、获取、RGB/HSV/HSL转换

#### 4. 图像管理测试
- **image_management_test** - 图像创建、删除、复制、尺寸等

#### 5. 窗口管理测试
- **window_management_test** - 窗口初始化、标题、可见性、视口等

### 性能测试 (Performance Tests)

#### 1. 图像操作性能
- **putimage_performance_test** - putimage系列函数性能基准
- **putimage_alphablend_test** - Alpha混合性能详细测试

#### 2. 图形绘制性能
- **drawing_performance_test** - 绘图函数性能测试

#### 3. 像素操作性能
- **pixel_operations_performance_test** - 像素级操作性能测试

## 构建说明

### 前置条件
1. 已构建的EGE图形库
2. CMake 3.13或更高版本
3. MinGW或Visual Studio编译器

### 构建步骤

1. **构建EGE库和测试**
   ```bash
   cd /path/to/xege
   mkdir build
   cd build
   
   # 构建所有测试
   cmake .. -DEGE_BUILD_TEST=ON
   cmake --build .
   ```

2. **构建选项**
   ```bash
   # 只构建功能性测试
   cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_PERFORMANCE=OFF -DEGE_TEST_FUNCTIONAL=ON
   
   # 只构建性能测试
   cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_PERFORMANCE=ON -DEGE_TEST_FUNCTIONAL=OFF
   
   # 构建所有测试（默认）
   cmake .. -DEGE_BUILD_TEST=ON -DEGE_TEST_PERFORMANCE=ON -DEGE_TEST_FUNCTIONAL=ON
   ```

3. **运行测试**
   ```bash
   # 运行所有测试
   ctest
   
   # 只运行功能性测试
   ctest -L functional
   
   # 只运行性能测试
   ctest -L performance
   
   # 运行特定测试
   ctest -R functional_drawing_primitives
   
   # 详细输出
   ctest -V
   
   # 直接运行测试程序
   ./bin/drawing_primitives_test
   ./bin/drawing_performance_test
   ```

## 测试分辨率

测试套件自动在以下分辨率下进行测试：

| 分辨率类别 | 尺寸 | 说明 |
|-----------|------|------|
| 极小 | 64x64 | 基础性能基准 |
| 小 | 128x128 | 低分辨率应用 |
| 标准 | 640x480 | 传统VGA |
| 高清 | 1280x720 | HD Ready |
| 全高清 | 1920x1080 | Full HD |
| 2K | 2560x1440 | QHD |
| 4K | 3840x2160 | Ultra HD |
| 8K | 7680x4320 | 8K UHD |

## 性能指标

每个测试提供以下性能指标：

- **平均执行时间** (毫秒) - 单次操作的平均耗时
- **帧率 (FPS)** - 每秒可执行的操作次数
- **操作/秒** - 吞吐量指标
- **最小/最大时间** - 性能变化范围
- **标准差** - 性能稳定性指标

## 测试图案类型

ImageGenerator支持以下测试图案：

1. **PATTERN_SOLID** - 纯色填充
2. **PATTERN_GRADIENT** - 渐变效果
3. **PATTERN_NOISE** - 随机噪点
4. **PATTERN_CHECKERBOARD** - 棋盘格
5. **PATTERN_LINES** - 线条图案
6. **PATTERN_CIRCLES** - 圆形图案
7. **PATTERN_MIXED** - 混合图案

## 使用示例

### 快速性能测试
```bash
# 运行完整的性能基准测试
./bin/putimage_performance_test.exe

# 只测试Alpha混合性能
./bin/putimage_alphablend_test.exe
```

### 自定义测试
可以修改测试参数：
- 调整迭代次数以获得更准确的结果
- 修改测试分辨率范围
- 添加新的测试图案或场景

## 输出示例

```
putimage Performance Test
========================

Testing resolution: 1920x1080
  Basic putimage test:
    Average time: 2.34 ms
    FPS: 427.35
    Operations/sec: 427.35

  Alphablend test (alpha=128):
    Average time: 4.67 ms
    FPS: 214.13
    Operations/sec: 214.13
    
...
```

## 注意事项

1. **图形窗口隐藏** - 测试期间图形窗口将被隐藏以避免干扰
2. **控制台保持** - 通过SHOW_CONSOLE=1宏保持控制台窗口可见
3. **性能影响** - 其他运行的程序可能影响测试结果
4. **内存要求** - 高分辨率测试需要足够的内存

## 故障排除

### 常见问题

1. **EGE库未找到**
   ```
   解决方案：确保EGE库已正确构建并位于预期路径
   ```

2. **链接错误**
   ```
   解决方案：检查系统库依赖，确保Windows SDK已安装
   ```

3. **运行时错误**
   ```
   解决方案：确保有足够的内存和图形资源
   ```

## 扩展测试

要添加新的测试用例：

1. 在`tests/`目录创建新的.cpp文件
2. 在CMakeLists.txt中添加新的可执行文件配置
3. 使用TestFramework和PerformanceTimer类进行测试
4. 参考现有测试文件的结构

## 性能优化建议

基于测试结果，可以考虑以下优化方向：

- **内存访问模式优化** - 减少缓存未命中
- **SIMD指令使用** - 利用向量化计算
- **多线程优化** - 并行处理像素数据
- **硬件加速** - 利用GPU加速

## 贡献

欢迎提交新的测试用例和性能优化建议！

## 许可证

本测试套件遵循EGE图形库的许可证条款。
