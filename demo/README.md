# 存放一些演示用的案例

按如下规则存放:

- 游戏类以 `game_` 前缀
- 图形渲染展示以 `graph_` 前缀
- 基础功能演示/测试以 `test_` 前缀
- 带相机功能类演示以 `camera_` 前缀

目录规则:

- `compile-tests/` 目录下存放用于编译测试的一些文件, 仅用于验证编译，不具备演示功能

`graph_backend_validation.cpp` 是确定性的 GDI/OpenGL 固定帧对比用例。它不读取外部资源或
随机数，运行到第 10 帧后保存截图并自动退出；完整构建与比较方法见 `tests/README.md`。
