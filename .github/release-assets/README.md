# EGE 发布制品说明 / Release Artifacts Documentation

本目录包含用于创建 EGE 发布包的资源文件。

## 文件说明

### CMakeLists.txt

这是发布包根目录的 CMake 配置文件，用于让用户能够直接使用发布包编译示例程序。

**特性：**
- 自动检测编译器类型和版本
- 自动选择对应的静态库目录
- 支持 MSVC (VS2017-VS2026) 和 MinGW-w64 编译器
- 支持 Linux/macOS 交叉编译环境
- 包含 demo 目录下所有示例程序的编译配置

**库目录映射：**
- MSVC: 根据编译器版本自动选择 `lib/vs2017/`, `lib/vs2019/`, `lib/vs2022/` 等
- MinGW (Windows): 默认使用 `lib/mingw64/`
- MinGW (Ubuntu): 使用 `lib/mingw-w64-debian/`
- MinGW (macOS): 使用 `lib/macOS/`
- Code::Blocks: 使用 `lib/codeblocks/`
- CLion/小熊猫C++: 使用 `lib/redpanda/`

## 发布包结构

发布包（如 `ege-25.11.7z`）解压后的目录结构如下：

```
ege-25.11/
├── CMakeLists.txt          # 根目录 CMake 配置（来自本目录）
├── demo/                   # 示例程序源代码（不含编译配置）
│   ├── *.cpp               # 示例程序
│   ├── *.rc                # 资源文件
│   ├── *.jpg, *.png        # 图片资源
│   └── gmp-demo/           # GMP 示例（如果有）
├── doc/                    # 文档
├── include/                # 头文件
│   └── ege/                # EGE 头文件
├── lib/                    # 静态库
│   ├── vs2022/x64/         # Visual Studio 2022 静态库
│   ├── vs2019/x64/         # Visual Studio 2019 静态库
│   ├── vs2017/x64/         # Visual Studio 2017 静态库
│   ├── mingw64/            # MinGW-w64 (MSYS2) 静态库
│   ├── codeblocks/         # Code::Blocks 25.03 静态库
│   ├── redpanda/           # CLion/小熊猫C++ 静态库
│   ├── mingw-w64-debian/   # Ubuntu 交叉编译静态库
│   └── macOS/              # macOS 交叉编译静态库
├── man/                    # API 文档
├── egelogo.jpg             # EGE Logo
├── version.txt             # 版本号
└── 说明.txt                # README (README.md 的副本)
```

## 使用发布包

用户下载并解压发布包后，可以直接使用 CMake 编译示例程序：

```bash
cd ege-25.11
mkdir build
cd build
cmake ..
cmake --build .
```

或者在 Visual Studio 中直接打开 `CMakeLists.txt`。

## 发布流程

### 正式发布（推荐）

1. 更新 `include/ege.h` 中的版本号（`EGE_VERSION_MAJOR`, `EGE_VERSION_MINOR`, `EGE_VERSION_PATCH`）

2. 推送符合规则的版本标签：
   ```bash
   git tag v25.11      # 格式: v{major}.{minor}[.{patch}][-suffix]
   git push origin v25.11
   ```

3. GitHub Actions 会自动：
   - **验证标签格式和版本号**
   - 在各个平台上编译所有版本的静态库
   - 组织文件到发布包结构
   - 创建 `.7z` 和 `.zip` 压缩包
   - 创建 GitHub Release（草稿模式）

4. 在 GitHub Release 页面检查并发布

**重要提示：**
- 标签版本必须与 `include/ege.h` 中的版本匹配
- 标签必须在 master 分支上
- 标签格式: `/^v(\d+)\.(\d+)(\.\d+)?(-\w+)?$/`

### 测试发布

1. 在 GitHub Actions 页面，选择 "Release Package" 工作流
2. 点击 "Run workflow"
3. 输入测试版本号（如 `25.11-test`）
4. 点击 "Run workflow" 开始

**注意：** 测试发布不会创建 GitHub Release，制品可在 Actions 页面下载。

## 开发说明

### 修改 CMakeLists.txt

如果需要修改发布包的 CMakeLists.txt：

1. 编辑 `.github/release-assets/CMakeLists.txt`
2. 提交更改
3. 下次发布时会自动使用新的配置

### 修改发布流程

发布流程定义在 `.github/workflows/release.yml` 中。主要组成部分：

1. **build-msvc-libraries**: 编译 MSVC 版本静态库
2. **build-mingw-windows-libraries**: 编译 MinGW Windows 版本静态库
3. **build-cross-libraries**: 编译交叉编译版本静态库
4. **create-release-package**: 组装发布包并创建压缩包

### 添加新的编译器支持

要添加新的编译器版本支持：

1. 在 `release.yml` 中添加对应的构建任务
2. 在 `CMakeLists.txt` 中添加库路径映射
3. 更新本文档

## 注意事项

1. **库文件命名**: 
   - MSVC: `graphics.lib`, `graphicsd.lib` (Debug版本)
   - MinGW: `libgraphics.a`

2. **子模块**: 发布包不包含源码编译需要的子模块（如 3rdparty）

3. **Demo 文件**: 发布包中的 demo 目录不包含 `CMakeLists.txt` 和 `ege_release.cmake`，这些文件仅用于源码编译

4. **版本兼容性**: 
   - VS2022/2019/2017 的静态库相互兼容
   - 不同 MinGW 版本的静态库可能不兼容，因此分别提供

## 相关链接

- [xege-sdk 仓库](https://github.com/x-ege/xege-sdk) - 旧的发布仓库（参考）
- [EGE 官网](https://xege.org)
- [构建指南](../../BUILD.md)
