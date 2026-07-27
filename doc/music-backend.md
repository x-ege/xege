# MUSIC 跨平台后端

## 实现矩阵

| 平台 | 默认实现 | 格式与说明 |
| --- | --- | --- |
| Windows | MCI | 保留原有实现、返回值和格式行为 |
| macOS | AVFAudio | 普通音频使用 `AVAudioPlayer`；MIDI 使用 `AVAudioEngine`、`AVAudioSequencer` 和系统 DLSSynth |
| Linux（检测到 GStreamer） | GStreamer，失败时回退 miniaudio | 格式取决于已安装的 GStreamer 插件；MIDI 需要 MIDI 解码插件和 soundfont |
| Linux（无 GStreamer 开发包） | 内置 miniaudio | 无额外编译依赖，支持 WAV、MP3 和 FLAC；不解码 MIDI |

所有后端都实现 `OpenFile`、`Play`、`RepeatPlay`、`Pause`、`Stop`、`Seek`、
`SetVolume`、`Close`、`GetPosition`、`GetLength` 和 `GetPlayStatus`。区间播放和
区间循环统一使用毫秒；`Close` 保持幂等。

## ABI 兼容

`MUSIC` 公共类仍然只有原来的 `m_DID` 和 `m_dwCallBack` 两个数据成员。非 Windows
后端对象由 `music.cpp` 的内部注册表管理，因此没有改变类大小、字段偏移、公开函数
签名或 Windows MCI 路径。两个公开头文件继续保持同一组声明。

## Linux 构建选项

`EGE_MUSIC_GSTREAMER` 接受三个值：

- `AUTO`（默认）：有 `gstreamer-1.0` 开发包时编译 GStreamer，并保留 miniaudio
  运行时回退；没有开发包时只编译 miniaudio。
- `ON`：要求 GStreamer 开发包存在，否则 CMake 配置失败。
- `OFF`：只编译 miniaudio，适合最小化或完全自包含的构建。

例如：

```sh
cmake -S . -B build -DEGE_MUSIC_GSTREAMER=ON
```

Ubuntu 上可选的 GStreamer 支持通常需要 `libgstreamer1.0-dev`；
`gstreamer1.0-plugins-base` 和 `gstreamer1.0-plugins-good` 提供常见音频格式。
MIDI 还需要 `gstreamer1.0-plugins-bad` 中的 FluidSynth/WildMIDI 解码器和可用的
soundfont。没有这些运行时插件时，普通音频仍可播放，MIDI 的 `OpenFile` 会明确失败。

当两种 Linux 后端都编译时，可设置 `EGE_MUSIC_BACKEND=miniaudio` 强制使用回退
后端，便于部署诊断和测试。

## Linux 音频栈

GStreamer 的自动音频输出会使用系统中可用的 PipeWire、PulseAudio 或 ALSA
元素。miniaudio 会直接探测 PulseAudio、JACK 或 ALSA；在以 PipeWire 提供
PulseAudio 兼容服务的桌面上也可正常工作。Linux 没有一个在所有发行版上都保证存在
且同时负责解码、MIDI 合成和设备输出的系统 framework，因此采用“桌面集成优先、
自包含回退”的两级方案。

测试可设置 `EGE_MUSIC_AUDIO_BACKEND=null`，让 GStreamer 使用同步 `fakesink`、
miniaudio 使用 null device。这样 CI 能验证真实解码、定位、状态和循环，而不依赖声卡。

## miniaudio 版本

仓库通过子模块固定 miniaudio `0.11.25`（MIT No Attribution / Unlicense 双许可）。
克隆仓库时应初始化子模块：

```sh
git submodule update --init --recursive
```
