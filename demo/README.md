# 存放一些演示用的案例

按如下规则存放:

- 游戏类以 `game_` 前缀
- 图形渲染展示以 `graph_` 前缀
- 基础功能演示/测试以 `test_` 前缀
- 带相机功能类演示以 `camera_` 前缀

目录规则:

- `compile-tests/` 目录下存放用于编译测试的一些文件, 仅用于验证编译，不具备演示功能

## 构建与运行

在仓库根目录执行：

```sh
cmake -S . -B build/demos \
  -DCMAKE_BUILD_TYPE=Release \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEST=OFF \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF
cmake --build build/demos --target demos --parallel
```

macOS 会生成无扩展名的 Mach-O 程序，例如
`build/demos/demo/graph_5star`；Windows 生成 `.exe`。`graph_star` 使用 Win32
屏保预览接口，只在 Windows 构建。Unix 主机不构建 `gmp-demo/`。

如需 camera demo，先初始化子模块并重新配置：

```sh
git submodule update --init --recursive 3rdparty/ccap
cmake -S . -B build/demos -DEGE_BUILD_DEMO=ON -DEGE_ENABLE_CAMERA_CAPTURE=ON
cmake --build build/demos --target camera_base camera_wave --parallel
```

macOS camera 可执行文件已内嵌 `NSCameraUsageDescription`；首次运行会由
系统请求权限。自动测试只做 camera 编译/链接，不会打开设备。
