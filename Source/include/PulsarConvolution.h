#pragma once

#include <vector>

namespace why {

/**
 * Curtis Roads 描述的 Pulsar 卷积实现
 * 
 * 核心概念：
 * - infrasonic pulsar train 的每个脉冲触发一个 sample object 的拷贝
 * - 短 IR（pulsar 周期 < sample 时长）= 节奏 pattern
 * - 长 IR（重叠）= 滤波/混响效果
 */
class PulsarConvolution {
public:
  void prepare(double sampleRate, int maxBlockSize);
  
  /**
   * 处理一帧
   * @param pulsarSample 当前 pulsar 振幅（单个 sample，不是 buffer）
   * @param sourceSample 当前 source sample
   * @return 卷积输出
   */
  float processSample(float pulsarSample, float sourceSample);
  
  /**
   * 设置 sample object 长度（决定是节奏模式还是连续纹理）
   * @param durationMs sample 时长（ms），< pulsar 周期 = 节奏，> pulsar 周期 = 重叠
   */
  void setSampleObjectDuration(float durationMs);
  
  /**
   * 设置 pulsar train 的当前参数（用于计算 IR 长度）
   */
  void setPulsarPeriod(float periodMs);  // 1/fundamental frequency
  
private:
  // 环形缓冲区存储最近的 source samples（作为 FIR 抽头）
  std::vector<float> sourceHistory;
  size_t writeIndex = 0;
  
  // pulsar 触发检测状态
  float lastPulsarValue = 0.0f;
  bool wasAboveThreshold = false;
  
  // 当前激活的 "sample object 拷贝" 状态
  struct ActiveGrain {
    float amplitude = 0.0f;  // pulsar 触发时的 amplitude
    int samplesRemaining = 0;  // 剩余播放 samples
    int sourceReadHead = 0;    // 在 sourceHistory 中的读取位置
  };
  std::vector<ActiveGrain> activeGrains;
  
  float sampleRate = 44100.0f;
  int maxHistorySize = 0;  // 由 sample object duration 决定
  float triggerThreshold = 0.01f;
};

} // namespace why
