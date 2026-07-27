# MUSIC 跨平台后端方案

## 当前契约

Windows 继续使用 MCI，保持既有格式支持、返回值和 ABI。Linux/macOS 目前没有播放
后端；这些平台会明确返回 `MUSIC_ERROR`，而不是像旧占位实现那样报告成功却始终处于
未打开状态。`Close()` 对未打开对象仍然成功，`GetPlayStatus()` 返回
`MUSIC_MODE_NOT_OPEN`。

## 方案比较

| 方案 | 优点 | 代价与限制 |
| --- | --- | --- |
| 内置 miniaudio（推荐） | 单头文件、许可宽松、WAV/MP3/FLAC 解码和 Linux/macOS/Windows 输出统一；可避免要求用户安装开发包 | 需要维护 vendored 版本；MIDI 不在核心解码范围；仍需验证 ALSA/PulseAudio/PipeWire 运行时 |
| SDL2_mixer | API 成熟，格式覆盖较广 | 给图形库增加 SDL2/SDL_mixer 系统依赖，和现有 GLFW 窗口栈无关且偏重 |
| GStreamer | Linux 桌面格式和设备支持最广 | 开发包、运行时插件和部署复杂，难以维持 EGE 的轻量依赖模型 |
| 外部播放器进程 | 实现快 | 生命周期、定位、音量、错误传播和安全转义都不可靠，不适合作为库后端 |

## 推荐落地顺序

1. 保持 `MUSIC` 的公开类布局和返回类型不变，在实现内部增加平台无关的私有播放状态。
2. vendor 固定版本的 miniaudio，先实现 WAV、MP3、FLAC 的
   `OpenFile/Play/Pause/Stop/Seek/SetVolume`，并增加生成式短音频 fixture。
3. Linux CI 使用 miniaudio 的 null 后端验证解码、定位、循环、时长和状态机；有声卡
   环境仅作为可选集成测试，避免 hosted runner 的设备差异造成不稳定。
4. Ogg/Vorbis 可通过 miniaudio 自定义 decoder 接入；MIDI 建议作为可选的
   FluidSynth 后端，需要显式 soundfont，不让它成为核心构建依赖。
5. 后端成熟后再考虑 Windows 默认切换；在此之前 Windows 继续走 MCI，避免改变旧程序
   的 codec、MIDI 和设备行为。

这个拆分能先消除 Linux 的假成功，再用轻量且可测试的后端逐步补齐真实播放，而不破坏
现有 Windows ABI。
