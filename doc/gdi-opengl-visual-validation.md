# Windows GDI / OpenGL 固定帧评测报告

评测日期：2026-07-31

代码基线：`30c3c934097d3e4f9456d2fbfbee82972bb7495e`

结论：四个固定帧用例全部通过；发现并修复一处 OpenGL viewport 清屏兼容问题。

## 环境与方法

- Windows，Visual Studio 17 2022，Windows SDK 10.0.26100.0。
- GDI 与 OpenGL 使用互不复用的 Release 构建目录，分别强制对应运行时后端并关闭开屏动画。
- 每个程序运行到确定的第 10 帧，通过 EGE 自身的 `getimage`/`saveimage` 保存 640×480 PNG。
- 截图保存前统一把最终帧 alpha 设为 255，只比较窗口最终可见的 RGB，避免文件编码阶段再次
  反预乘而提亮半透明像素。
- 逐像素比较三个 RGB 通道的绝对差。报告给出平均绝对误差（MAE）、任一通道误差至少 33 的
  大差异像素比例，以及增强四倍亮度的差异图。
- 普通 demo 的门槛是 MAE ≤ 2、强差异比例 ≤ 3%。专用验证图边缘密度更高，并保留一项
  已确认的旧 GDI 填充映射差异，因此单独使用 MAE ≤ 5.5、强差异比例 ≤ 6%。

## 最终结果

| 用例 | 主要覆盖 | MAE | 变化像素 | 强差异像素 | 判定 |
| --- | --- | ---: | ---: | ---: | --- |
| `graph_alpha` | alpha 图元 | 0.0049 | 0.0091% | 0.0091% | 通过 |
| `graph_backend_validation` | 图元、线型、填充、viewport、PRGB/颜色键、旋转、缩放、文字 | 4.9672 | 8.5286% | 5.3503% | 通过专用门槛 |
| `graph_ball` | 固定随机种子的动画圆形 | 0.3902 | 0.7028% | 0.5550% | 通过 |
| `test_demo` | 复杂组合图形、文字和曲线 | 0.6400 | 0.7288% | 0.6940% | 通过 |

机器可读结果及两端原图、增强差异图位于本地生成目录
`build/visual-results/final-a/`；该构建输出不纳入版本库。

视觉复核结论：

- `graph_alpha` 的差异只出现在椭圆边缘少量抗锯齿像素。
- `graph_ball` 的圆心、半径、颜色和位置一致，差异沿一像素圆周分布。
- `test_demo` 的布局、填充和曲线几何一致，差异集中在文字与曲线抗锯齿边缘。
- 专用验证图的实体几何、viewport 裁剪、固定 PRGB 合成、颜色键复制、RGB32 全局 alpha、
  旋转和缩放结果一致。较高的统计值主要来自边缘密集的文字/曲线，以及下述旧 GDI
  `WIDE_DOT_FILL` 映射。

## 发现与处理

### 已修复：OpenGL `cleardevice` 被 viewport 裁剪

旧 GDI 的 `cleardevice` 会清除完整目标；OpenGL 在绑定绘图目标时把活动 viewport 设置为
GL scissor，随后直接调用 `glClear`，导致 viewport 外像素没有清除。

先加入最小回归：在 viewport 外预置红色像素，切换背景为蓝色后调用 `cleardevice`。修复前
OpenGL 测试稳定读回红色，GDI 读回蓝色。修复在完整目标清屏期间临时禁用 scissor，并在
清屏后恢复原状态；修复后 GDI/OpenGL 两端均读回蓝色。

### 评测输入修正：alpha 源与截图格式

`putimage_withalpha` 遵循 Win32 AlphaBlend 的 PRGB 预乘契约。最初的实验图用增强图元生成
源图后再假设它是直通 ARGB，混入了源图生成路径的格式差异。最终用例改成直接写入固定的
PRGB 像素，分别验证传输结果。

屏幕抓取缓冲中的 alpha 也不能直接作为非 alpha PNG 的预乘依据。四个固定帧 demo 现在都在
保存前把截图标记为不透明，保留真正显示的 RGB。

### 保留并记录：旧 GDI 的 `WIDE_DOT_FILL` 映射不符合枚举说明

公开头文件把 `WIDE_DOT_FILL` 定义为稀疏点填充。OpenGL 输出稀疏点阵；旧 GDI 的未完整
`hatchmap` 却把它映射到 `HS_DIAGCROSS`，输出斜交叉网格。这里判断新实现正确，未为了压低
差异而把 OpenGL 改回旧错误。专用用例的独立阈值明确包含了这项已知差异。

## 稳定性与回归门禁

- 完整评测独立运行两次，GDI/OpenGL 共八张原始截图的 SHA-256 分别逐张一致。
- `graph_ball` 固定使用种子 42；专用验证图无随机数或外部资源。
- Debug 功能测试：GDI 17/17，OpenGL 构建 28/28。
- Release 功能测试：GDI 17/17，OpenGL 构建 28/28。
- 两套 Release `demos` 目标均完整构建；四个固定帧用例在两轮评测中均通过。

构建中仍可看到项目既有的整数/浮点转换和 deprecated API 警告，本次没有新增构建失败或
测试报错。
