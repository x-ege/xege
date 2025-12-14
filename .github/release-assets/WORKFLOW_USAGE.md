# EGE 发布工作流使用指南 / Release Workflow Usage Guide

## 概述 / Overview

本工作流自动化构建 EGE 的发布包，替代了之前使用 Git 管理的 xege-sdk 仓库。

This workflow automates the creation of EGE release packages, replacing the previous xege-sdk Git repository approach.

## 触发方式 / Triggering Methods

### 方法 1: 正式发布 (Official Release) - 推荐 / Method 1: Official Release (Recommended)

推送符合规则的版本标签时自动触发：

```bash
git tag v25.11      # 或 v25.11.0, v25.11-rc 等
git push origin v25.11
```

**标签格式规则 / Tag Format Rules:**
- 格式: `v{major}.{minor}[.{patch}][-suffix]`
- 示例 / Examples: `v25.11`, `v25.11.0`, `v25.11-rc`, `v25.11.1-beta`
- 正则表达式 / Regex: `/^v([0-9]+)\.([0-9]+)(\.([0-9]+))?(-[a-zA-Z0-9_]+)?$/`

**验证流程 / Validation Process:**
1. 验证标签格式是否正确
2. 验证标签版本与 `include/ege.h` 中的 `EGE_VERSION` 是否匹配
3. 验证标签是否存在于 master 分支
4. 构建所有平台的库文件
5. 创建发布包
6. 创建 GitHub Release（草稿模式）

The workflow will:
1. Validate tag format
2. Verify tag version matches `EGE_VERSION` in `include/ege.h`
3. Ensure tag is on master branch
4. Build libraries for all platforms
5. Create release packages
6. Create a GitHub Release (draft mode)

### 方法 2: 测试发布 (Test Release) - 手动触发 / Method 2: Test Release (Manual)

1. 访问 GitHub Actions 页面
2. 选择 "Release Package" 工作流
3. 点击 "Run workflow"
4. 输入测试版本号（如 `25.11-test` 或 `25.11.1-rc`）
5. 点击 "Run workflow"

**注意 / Notes:**
- 测试发布**不会**创建 GitHub Release
- 最终发布包制品可在 Actions 页面下载（保留 90 天）
- 适用于测试和验证发布流程

Test releases:
- Will **NOT** create a GitHub Release
- Final release package artifacts available on Actions page (90 days retention)
- Suitable for testing and validation

### 方法 3: 自动测试发布 / Method 3: Automatic Test Release

当 master 分支更新时（非标签推送），自动触发测试发布：

```bash
git push origin master
```

工作流会使用 `include/ege.h` 中的版本号加上 commit SHA 创建测试发布：
- 版本格式: `{version}-dev-{short_sha}`
- 示例: `25.11-dev-a1b2c3d`
- 不创建 GitHub Release

When master branch is updated (not a tag push), a test release is automatically triggered:
- Version format: `{version}-dev-{short_sha}`
- Example: `25.11-dev-a1b2c3d`
- No GitHub Release created

## 工作流程 / Workflow Process

### 阶段 1: 构建库文件 / Stage 1: Build Libraries

并行构建多个平台的库文件：

**MSVC 库:**
- VS2022 (toolset v143) - Windows latest
- VS2019 (toolset v142) - Windows 2019
- VS2017 (toolset v141) - Windows 2019

**MinGW Windows 库:**
- MSYS2 最新版 (GCC 最新版) / MSYS2 Latest (GCC Latest Version)
- Code::Blocks GCC 14.2.0
- CLion/小熊猫C++ (RedPanda C++) GCC 13.1.0

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

### 正式发布流程 / Official Release Process

### 1. 准备发布 / Prepare Release

确保：
- 代码已合并到 master 分支
- RELEASE.md 已更新
- **`include/ege.h` 中的 `EGE_VERSION_MAJOR`, `EGE_VERSION_MINOR`, `EGE_VERSION_PATCH` 已更新**
- 版本号已确定

Ensure:
- Code is merged to master
- RELEASE.md is updated
- **`EGE_VERSION_MAJOR`, `EGE_VERSION_MINOR`, `EGE_VERSION_PATCH` in `include/ege.h` are updated**
- Version number is determined

### 2. 创建并推送标签 / Create and Push Tag

```bash
# 确保在 master 分支
git checkout master
git pull

# 创建标签（版本号必须与 ege.h 匹配）
git tag v25.11      # 如果 ege.h 中是 25.11.0
# 或
git tag v25.11.1    # 如果 ege.h 中是 25.11.1

# 推送标签
git push origin v25.11
```

### 3. 等待构建 / Wait for Build

工作流需要约 30-60 分钟完成所有构建。

The workflow takes approximately 30-60 minutes to complete all builds.

**监控构建 / Monitor Build:**
1. 访问 Actions 页面
2. 查看 "Release Package" 工作流
3. 检查验证步骤是否通过

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

### 测试发布流程 / Test Release Process

1. 手动触发工作流（见方法 2）
2. 等待构建完成
3. 在 Actions 页面下载制品
4. 本地测试发布包

## 故障排查 / Troubleshooting

### 验证失败 / Validation Failures

#### 1. 标签格式错误 / Tag Format Error
```
Error: Tag 'vXXX' does not match required format
```
**解决方法 / Solution:**
- 使用正确格式: `v{major}.{minor}[.{patch}][-suffix]`
- 示例: `v25.11`, `v25.11.0`, `v25.11-rc`

#### 2. 版本不匹配 / Version Mismatch
```
Error: Tag version (X.X.X) does not match EGE_VERSION in ege.h (Y.Y.Y)
```
**解决方法 / Solution:**
1. 检查 `include/ege.h` 中的版本号
2. 更新头文件版本号或使用匹配的标签
3. 确保 `EGE_VERSION_MAJOR`, `EGE_VERSION_MINOR`, `EGE_VERSION_PATCH` 都正确

#### 3. 标签不在 master 分支 / Tag Not On Master
```
Error: Tag 'vX.X' is not on the master branch
```
**解决方法 / Solution:**
1. 切换到 master 分支
2. 确保代码已合并到 master
3. 从 master 分支创建标签

### 库文件缺失 / Missing Libraries

检查构建日志，查看是否有编译错误：

```
查看 Actions -> Release Package -> [对应的 job]
```

### 工作流失败 / Workflow Failure

常见原因：
1. **验证错误**: 检查标签格式和版本号
2. **编译错误**: 检查源代码是否有问题
3. **依赖下载失败**: 网络问题，重新运行工作流
4. **权限问题**: 确保 GITHUB_TOKEN 有正确权限

Common causes:
1. **Validation errors**: Check tag format and version number
2. **Compilation errors**: Check source code
3. **Download failures**: Network issues, re-run workflow
4. **Permission issues**: Ensure GITHUB_TOKEN has correct permissions

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
