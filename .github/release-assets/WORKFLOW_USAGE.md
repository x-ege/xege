# EGE 发布工作流使用指南 / Release Workflow Usage Guide

## 概述 / Overview

本工作流自动化构建 EGE 的发布包，替代了之前使用 Git 管理的 xege-sdk 仓库。

This workflow automates the creation of EGE release packages, replacing the previous xege-sdk Git repository approach.

## 触发方式 / Triggering Methods

### 方法 1: 自动触发（推荐） / Method 1: Automatic (Recommended)

推送版本标签时自动触发：

```bash
git tag v25.11
git push origin v25.11
```

工作流会自动：
1. 构建所有平台的库文件
2. 创建发布包
3. 创建 GitHub Release（草稿模式）

The workflow will automatically:
1. Build libraries for all platforms
2. Create release packages
3. Create a GitHub Release (draft mode)

### 方法 2: 手动触发 / Method 2: Manual

1. 访问 GitHub Actions 页面
2. 选择 "Release Package" 工作流
3. 点击 "Run workflow"
4. 输入版本号（如 `25.11`）
5. 选择是否创建 GitHub Release
6. 点击 "Run workflow"

Visit GitHub Actions page, select "Release Package" workflow, click "Run workflow", enter version number, and run.

## 工作流程 / Workflow Process

### 阶段 1: 构建库文件 / Stage 1: Build Libraries

并行构建多个平台的库文件：

**MSVC 库:**
- VS2022 (toolset v143) - Windows latest
- VS2019 (toolset v142) - Windows 2019
- VS2017 (toolset v141) - Windows 2019

**MinGW Windows 库:**
- MSYS2 Latest (GCC 最新版)
- Code::Blocks GCC 14.2.0
- CLion/RedPanda GCC 13.1.0

**交叉编译库:**
- Ubuntu MinGW-w64
- macOS MinGW-w64

### 阶段 2: 创建发布包 / Stage 2: Create Release Package

工作流会：
1. 下载所有构建的库文件
2. 创建发布包目录结构
3. 复制文件到正确位置
4. 验证库文件数量
5. 创建压缩包（.7z 和 .zip）
6. 上传到 GitHub

The workflow:
1. Downloads all built libraries
2. Creates release package structure
3. Copies files to correct locations
4. Validates library file counts
5. Creates archives (.7z and .zip)
6. Uploads to GitHub

## 发布包结构 / Package Structure

```
ege-{version}/
├── CMakeLists.txt           # 根 CMake 配置 / Root CMake config
├── demo/                    # 示例程序 / Demo programs
│   ├── *.cpp
│   ├── *.rc
│   └── gmp-demo/
├── doc/                     # 文档 / Documentation
├── include/                 # 头文件 / Header files
│   └── ege/
├── lib/                     # 库文件 / Library files
│   ├── vs2022/x64/         # Visual Studio 2022
│   ├── vs2019/x64/         # Visual Studio 2019
│   ├── vs2017/x64/         # Visual Studio 2017
│   ├── mingw64/            # MinGW MSYS2
│   ├── codeblocks/         # Code::Blocks
│   ├── redpanda/           # CLion/小熊猫C++
│   ├── mingw-w64-debian/   # Ubuntu cross-compile
│   └── macOS/              # macOS cross-compile
├── man/                     # API 文档 / API docs
├── egelogo.jpg
├── version.txt
└── 说明.txt                # README copy
```

## 发布流程 / Release Process

### 1. 准备发布 / Prepare Release

确保：
- 代码已合并到 master 分支
- RELEASE.md 已更新
- 版本号已确定

Ensure:
- Code is merged to master
- RELEASE.md is updated
- Version number is determined

### 2. 触发构建 / Trigger Build

使用上述任一方法触发工作流。

Use either method above to trigger the workflow.

### 3. 等待构建 / Wait for Build

工作流需要约 30-60 分钟完成所有构建。

The workflow takes approximately 30-60 minutes to complete all builds.

### 4. 检查草稿发布 / Check Draft Release

1. 访问 GitHub Releases 页面
2. 查看新创建的草稿发布
3. 检查：
   - 版本号是否正确
   - 文件是否完整（两个压缩包）
   - 发布说明是否合适

Check:
- Version number is correct
- Files are complete (two archives)
- Release notes are appropriate

### 5. 发布 / Publish

如果一切正常，点击 "Publish release" 按钮。

If everything looks good, click "Publish release".

## 故障排查 / Troubleshooting

### 库文件缺失 / Missing Libraries

检查构建日志，查看是否有编译错误：

```
查看 Actions -> Release Package -> [对应的 job]
```

### 工作流失败 / Workflow Failure

常见原因：
1. **编译错误**: 检查源代码是否有问题
2. **依赖下载失败**: 网络问题，重新运行工作流
3. **权限问题**: 确保 GITHUB_TOKEN 有正确权限

Common causes:
1. **Compilation errors**: Check source code
2. **Download failures**: Network issues, re-run workflow
3. **Permission issues**: Ensure GITHUB_TOKEN has correct permissions

### 制品缺失 / Missing Artifacts

如果某个平台的库文件缺失：
1. 检查对应 job 的日志
2. 查看是否有构建错误
3. 必要时手动构建该平台

If libraries for a platform are missing:
1. Check the corresponding job log
2. Look for build errors
3. Build manually if necessary

## 维护 / Maintenance

### 更新编译器版本 / Update Compiler Version

编辑 `.github/workflows/release.yml`：
1. 找到对应的 matrix 配置
2. 更新 GCC URL 或 toolset 版本
3. 提交更改

Edit `.github/workflows/release.yml`:
1. Find the corresponding matrix configuration
2. Update GCC URL or toolset version
3. Commit changes

### 添加新平台 / Add New Platform

1. 在 workflow 中添加新的 matrix 项
2. 更新 `.github/release-assets/CMakeLists.txt` 添加库路径映射
3. 更新文档

Add new matrix item in workflow, update CMakeLists.txt for library path mapping, and update documentation.

### 修改发布包结构 / Modify Package Structure

编辑 `.github/workflows/release.yml` 中的 "Create release package structure" 步骤。

Edit the "Create release package structure" step in `.github/workflows/release.yml`.

## 最佳实践 / Best Practices

1. **测试**: 在标签发布前先手动触发测试
2. **版本号**: 使用语义化版本号（如 25.11）
3. **发布说明**: 更新发布说明描述新特性和修复
4. **草稿模式**: 总是使用草稿模式，检查后再发布
5. **备份**: 保存发布包到安全位置

1. **Test**: Manually trigger before tagging
2. **Version**: Use semantic versioning
3. **Notes**: Update release notes with features and fixes
4. **Draft**: Always use draft mode and check before publishing
5. **Backup**: Save packages to safe location

## 相关资源 / Related Resources

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [发布包 CMakeLists.txt](./CMakeLists.txt)
- [发布包说明](./README.md)
- [构建指南](../../BUILD.md)
- [发布日志](../../RELEASE.md)
