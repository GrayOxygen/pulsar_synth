# Pulsar Synth（Pulsar Wang）

基于 **JUCE** 的实验性 **Pulsar / Pulsaret** 合成器插件。工程名 `pulsar_synth`，产品名 **「Pulsar Wang」**（v0.0.8）。最初在 **macOS + CLion** 下开发，当前 CMake 已适配 **Windows**（VST3 + Standalone）。

核心思路：用 **Train（脉冲列车）** 组织多个 **Pulsar 周期**，每个周期内再细分 **Pulse / IntraSilence**，最底层由可塑形的 **Pulsaret 波形** 发声，并可选 **卷积脉冲响应** 做音色塑形。

---

## 功能概览

### 触发方式（`Play Mode`）

| 模式 | 行为 |
|------|------|
| **Off**（NotSelected） | 默认未选择，静音 |
| **Auto** | 跟随 DAW 走带：播放开始触发 Train，停止则静音 |

引擎以单个 **`PulsarSynth` + `PulsarSynthVoice`** 渲染，仅由 **DAW 走带（Auto）** 触发。插件不接收 MIDI 输入（`NEEDS_MIDI_INPUT FALSE`）。

> 说明：旧版本曾提供 **MIDI Trigger** 模式（由 MIDI 音符触发），现已移除，相关双 Synth 架构亦简化为单 Synth。

### 时间与 Train 结构

- **BPM**：插件内部节拍（与 DAW 可独立），决定 `trainLenBlock`（默认 1/4 拍为一块）。
- **Train Len**：一列 Train 含多少个「拍块」。
- **Train Duty Cycle Len**：Train 发声段包含多少个 **Pulsar 周期**（pulse + intra-silence 交替）。
- **Train Silence Len**：Train 之间的静音段（以 Pulsar 周期个数计）。

关系（简化）：

```
trainPeriod = trainDutyCycleTime + trainSilenceTime
pulsarPeriod = trainTime / (dutyCycleLen + intervalSilenceLen)
```

参数变更时，通常等到 **Pulsar 静音** 或 **Train 间隔静音** 结束再切入新 Train，避免硬切（BPM / train 相关参数同理）。

### Pulsar / Pulsaret

- **Pg Waveform**：Pulsaret 波形表（LUT）在多种波形间平滑插值（正弦 → 复杂波 → 三角 → 锯齿 → 方波 → 随机等）。
- **Pg DutyCycle Ratio**：单个 Pulsar 周期内「发声」占空比。
- **Pg DutyCycle Cluster**：把一个 pulse 窗口再细分为多次脉冲簇（cluster > 1 时频率倍增）。
- **Pg Attack / Decay / Sustain / Release**：作用于 **整个 Pulsar 段**（含 mask 把 silence 提成 pulse 时）的 ADSR。

### 节奏 Mask（仅作用于 Train Duty Cycle 段）

| 类型 | 说明 |
|------|------|
| **Off** | 按原始 pulse / silence 输出 |
| **Burst Mask** | 用户字符串（如 `1011`），按 Pulsar 阶段索引选通 |
| **Euclid Mask** | 欧几里得节奏 `steps` / `hits` 生成 0/1 序列 |
| **Stochastic Mask** | 随 Train Duty Cycle 长度生成随机 0/1（长度约为 duty × 2，有上限） |

Mask 可把 **IntraSilence** 变成可听脉冲，并触发 ADSR / 频率重算（silence→pulse）。

### 调制

- **AM Waveform / FM Waveform**：在 Pulsaret 相位上，用另一组 LFO 波形表做幅度 / 音高（formant 频率）调制；波形可在表间插值。
- **Sustain** 同时参与 FM 深度缩放。

### 采样 Pulsaret（Impulse，Curtis Roads 方式）

本插件实现了 Curtis Roads 在《Microsound》中描述的 **Pulsar Synthesis** 理念。以下是两种实现方式的对比：

| 实现方式 | 当前代码 | 真正卷积（待实现） |
|----------|----------|-------------------|
| 方法 | 颗粒合成：每个 pulsar 周期扫描一遍采样 | 卷积：源信号与脉冲列车进行实时卷积 |
| 原理 | `sampleReadHead += sourceLength / periodSamples` | `output[n] = Σ source[n - k] * pulsarTrain[k]` |
| 包络 | ADSR 作用于扫描输出 | 脉冲本身的形状（如高斯）即为包络 |
| 时间拉伸 | 隐式（由 pulsar 周期决定扫描时间） | 显式通过卷积实现 |

**当前实现（Granular 扫描）**：

```cpp
// PulsarSynthVoice::calcActualPulse 中的采样读取
waveformSample = source.readLinear(sampleReadHead);
sampleReadHead += static_cast<float>(sourceLength) / periodSamples;
```

| 选项 | 说明 |
|------|------|
| **Off** | 使用内置 LUT 波形（`Pg Waveform`）作为 pulsaret |
| **Template** | 从 `Resources/*.wav` 加载为共享采样源 |
| **Sample** | 用户自选 WAV（Property 存路径，**不做自动化**） |

---

## Curtis Roads Pulsar Synthesis 原理

根据《Microsound》和《The Computer Music Tutorial》，真正的 Pulsar Synthesis 工作流程如下：

```
┌─────────────────────────────────────────────────────────────────┐
│                     PULSAR SYNTHESIS                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐      ┌─────────────────┐                      │
│  │  Source      │      │  Pulsar Train   │                     │
│  │  (任意音频)   │ ───► │  (脉冲列车)      │                     │
│  └──────────────┘      └─────────────────┘                      │
│         │                       │                               │
│         │              ┌────────┴────────┐                      │
│         │              │ 生成短脉冲序列    │                      │
│         │              │ (高斯/正弦突发)  │                      │
│         │              │ 频率 = 基础频率   │                      │
│         │              └────────┬────────┘                      │
│         │                       │                               │
│         ▼                       ▼                               │
│  ┌─────────────────────────────────────────┐                   │
│  │           CONVOLUTION (卷积)             │                   │
│  │  output[n] = Σ source[n - k] * train[k] │                   │
│  └─────────────────────────────────────────┘                   │
│                       │                                         │
│                       ▼                                         │
│                 ┌──────────┐                                    │
│                 │  Output  │                                    │
│                 └──────────┘                                    │
└─────────────────────────────────────────────────────────────────┘
```

**关键概念**：
- **Pulsar Train（脉冲列车）**：一系列短脉冲（通常 1-50ms），形状可为高斯、正弦突发等
- **Fundamental Frequency（基础频率）**：脉冲列车中相邻脉冲之间的时间间隔的倒数
- **Convolution（卷积）**：源信号与脉冲列车实时卷积，产生「时间采样」效果

**与颗粒合成的区别**：
- 颗粒合成：每个 grain 独立播放，有独立的起止时间和包络
- 卷积 pulsar：源信号被脉冲序列「采样」，不同延迟的源信号叠加产生干涉效应

---

## 信号流（音频线程）

```
processBlock
  → PulsarSynthEngine::processSample
       → PulsarSynth::renderNextBlockDirectly(…)
            → PulsarSynthVoice::processSampleWithConvolution
                 → 逐采样 processSample()
                      ├── 状态机：Pulse / IntraSilence / InterTrainSilence
                      ├── Mask 处理：Burst / Euclid / Stochastic
                      └── calcActualPulse()
                           ├── LUT 波形（内置）或
                           └── Sample 扫描（当前 granular 实现）
                 → voicePulseBuffer → tanh + gain → 叠加到主 buffer
```

### 预设与状态

- 参数走 **APVTS**；Mask 文本、Sample IR 路径、当前 Play Mode 等走 **ValueTree Property**。
- `getStateInformation` / `setStateInformation` 保存 XML；加载 preset 会 `reloadSynthPreset` 并广播 UI 更新。

### 输出

- **Output Gain**：-60 ~ +6 dB。
- 输出经 **tanh 软限幅**（快速近似）后叠加到立体声总线。

---

## 架构与目录

```
pulsar_synth/
├── CMakeLists.txt          # JUCE 插件、BinaryData、可选 PULSAR_ENABLE_PROFILING
├── Source/
│   ├── PluginProcessor.*   # APVTS、processBlock、参数监听
│   ├── PluginEditor.*      # 自定义 UI
│   ├── include/
│   │   ├── PulsarSynthEngine.h   # 单 Synth / 单 Voice、采样源
│   │   ├── PulsarSynth.h / PulsarSynthVoice.h
│   │   ├── CommonVoiceSate.h     # 多 Voice 共享参数与 Mask
│   │   ├── PulsaretSampleSource.h # 采样源（解码后的单声道 buffer）
│   │   ├── PulsaretWaveformSingleton.h / LfoWaveformSingleton.h
│   │   └── PulsarProfiler.h      # 可选性能统计
│   └── src/
│       └── PulsarSynthVoice.cpp  # Train 状态机 + 逐采样合成
├── Resources/              # 嵌入的模板 IR（*.wav）
├── Doxyfile / docs/        # API 文档
└── README.md
```

**数据流（音频线程）**

```
processBlock
  → PulsarSynthEngine::processSample（仅 Auto 模式）
       → PulsarSynth::renderNextBlockDirectly(…)
            → PulsarSynthVoice::processSampleWithConvolution
                 → 逐采样 processSample()（内置 LUT 波形）
                 → pulseBuffer → （可选卷积）→ tanh + gain → 叠加到主 buffer
```

---

## 性能优化（已实现）

针对逐采样热路径 `PulsarSynthVoice::processSample` / `processSampleWithConvolution`：

| 优化 | 内容 |
|------|------|
| **位置采样数移出热路径** | `resetTrainRelatedSamples4Location()` 不再逐采样调用，改为仅在 train/pulsar 配置变化时（`realChangeTrainConfig` / `resetTrain` / `parameterChanged`）重算 |
| **ADSR 去重复重算** | `refreshPulsaretAdsr()` 仅在采样率变化时 `setSampleRate`；当 ADSR 参数未变时跳过 `setParameters`（阶段内常量，等价于每阶段只算一次） |
| **output gain 缓存** | `getOutputGain()`（含 `std::pow`）每 block 只算一次，不再每采样/每声道重算 |
| **快速 tanh** | 输出软限幅由 `std::tanh` 改为 `juce::dsp::FastMathApproximations::tanh` |
| **mask 整数取模** | Burst / Euclid / Stochastic mask 索引由 `fmod` 改为整数 `%` |

**待做（P2）**：实现真正的卷积合成（基于 Curtis Roads 理论）；稳定 Pulsar 段内批量生成采样；LFO 单例按频率分桶。

---

## 环境（Windows）

| 项 | 路径 / 要求 |
|----|-------------|
| JUCE | 默认 `E:/env/juce-7.0.12-windows/JUCE`（`-DPULSAR_JUCE_DIR=...` 可改） |
| CMake | `G:\apps\CMake\bin\cmake.exe`（≥ 3.28） |
| 编译器 | VS 2022，工作负载「使用 C++ 的桌面开发」 |
| 标准 | C++20 |

插件格式：**VST3**、**Standalone**（Windows 不构建 AU）。

---

## 构建（Windows / Visual Studio）

本工程使用 **CMake + MSVC（Visual Studio 17 2022）** 构建，统一生成 **64 位（x64）** 插件。更详细步骤见 [BUILD_VS.md](BUILD_VS.md)。

### 命令行构建

```powershell
cd E:\coding\synth\pulsar_synth

# 1. 配置（指定 64 位 + JUCE 路径）
cmake -B build -G "MinGW Makefiles" -DPULSAR_JUCE_DIR="E:/env/juce-7.0.12-windows/JUCE"

# 2. Release 构建
cmake --build build --config Release

# Debug 构建
cmake --build build --config Debug

# 其它
cmake -B build -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

```


> JUCE 路径也可通过环境变量 `PULSAR_JUCE_DIR` 指定；未指定时默认 `E:/env/juce-7.0.12-windows/JUCE`。

### Visual Studio IDE 构建

1. VS 2022 中「文件 → 打开 → CMake…」，选择本工程目录。
2. 顶部配置选 `x64-Release` 或 `x64-Debug`，等待 CMake 配置完成。
3. 「生成 → 全部生成」（或 `Ctrl+Shift+B`）。

---

## 发布（Release）

- 使用 `--config Release` 构建，产物为优化后的 64 位二进制（已启用 JUCE 推荐的 LTO 标志）。
- 产物位置：
  - **VST3**：`build/pulsar_synth_artefacts/Release/VST3/Pulsar Wang.vst3`（架构目录为 `Contents/x86_64-win`，即 64 位）。
  - **Standalone**：`build/pulsar_synth_artefacts/Release/Standalone/Pulsar Wang.exe`。
- `COPY_PLUGIN_AFTER_BUILD TRUE`：构建后自动复制 VST3 到系统 VST3 目录。
- 若复制到 `Program Files` / 公共 VST3 目录报权限错误，可关闭 DAW、以管理员运行，或将 CMake 中 `COPY_PLUGIN_AFTER_BUILD` 设为 `FALSE`，直接使用 `build/.../VST3/` 下的产物。

---

## 运行

- **Standalone**：在 `build` 下搜索 `Pulsar Wang` / `Standalone` 的 `.exe`。
- **VST3**：在 DAW 中加载 `Pulsar Wang.vst3`；构建后可能已复制到系统公共 VST3 目录。
- **Play Mode** 设为 **Auto**，用 DAW 走带控制播放（开始播放即触发 Train，停止即静音）；设为 **Off** 则静音。

---

## CLion

打开工程后，CMake 可执行文件设为 `G:\apps\CMake\bin\cmake.exe`，工具链选 Visual Studio。构建目录常为 `cmake-build-*`（已在 `.gitignore`）。

---

## API 文档

```powershell
doxygen Doxyfile
```

输出：`docs/html/`。

---

## 许可

请自行补充项目许可。使用 JUCE 须遵守 [JUCE 许可条款](https://juce.com/juce-legal)（GPL 或商业许可等）。
