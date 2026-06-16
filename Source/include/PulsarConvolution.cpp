#include "PulsarConvolution.h"
#include <algorithm>

namespace why {

void PulsarConvolution::prepare(double sr, int maxBlockSize) {
  sampleRate = sr;
  // 默认最大 2 秒的 source history
  maxHistorySize = static_cast<int>(sampleRate * 2.0f);
  sourceHistory.resize(maxHistorySize, 0.0f);
  writeIndex = 0;
  lastPulsarValue = 0.0f;
  wasAboveThreshold = false;
  activeGrains.clear();
}

void PulsarConvolution::setSampleObjectDuration(float durationMs) {
  // 将 sample 时长转换为 samples
  maxHistorySize = static_cast<int>((durationMs / 1000.0f) * sampleRate);
  sourceHistory.resize(maxHistorySize, 0.0f);
}

float PulsarConvolution::processSample(float pulsarSample, float sourceSample) {
  // 1. 将 source sample 写入 history 环形缓冲区
  sourceHistory[writeIndex] = sourceSample;
  writeIndex = (writeIndex + 1) % maxHistorySize;
  
  // 2. 检测 pulsar 脉冲触发（过零或阈值检测）
  // 使用峰值检测：pulsar 从低到高越过阈值 = 新脉冲
  bool isAboveThreshold = pulsarSample > triggerThreshold;
  bool trigger = isAboveThreshold && !wasAboveThreshold;
  wasAboveThreshold = isAboveThreshold;
  
  float output = 0.0f;
  
  // 3. 触发新的 grain（"pulsar 被 sample object 替换"）
  if (trigger) {
    ActiveGrain grain;
    grain.amplitude = pulsarSample;  // 使用 pulsar 振幅作为 grain 增益
    grain.samplesRemaining = maxHistorySize;  // 播放整个 sample object
    grain.sourceReadHead = writeIndex;  // 从当前位置开始读
    activeGrains.push_back(grain);
  }
  
  // 4. 处理所有活跃的 grains（这是 convolution 的核心）
  for (auto it = activeGrains.begin(); it != activeGrains.end();) {
    if (it->samplesRemaining > 0) {
      // 从 sourceHistory 读取，带循环回绕
      int readIdx = (it->sourceReadHead) % maxHistorySize;
      float sample = sourceHistory[readIdx];
      
      // 可选：对 grain 尾部 fade out 避免爆音
      float fade = 1.0f;
      if (it->samplesRemaining < 100) {  // 最后 100 samples fade
        fade = it->samplesRemaining / 100.0f;
      }
      
      output += sample * it->amplitude * fade;
      
      it->sourceReadHead = (it->sourceReadHead + 1) % maxHistorySize;
      it->samplesRemaining--;
      ++it;
    } else {
      // 移除完成的 grain
      it = activeGrains.erase(it);
    }
  }
  
  // 5. 可选：混入原始 source（dry/wet 控制）
  // output = output + sourceSample * dryLevel;
  
  return output;
}

void PulsarConvolution::setPulsarPeriod(float periodMs) {
  // 可以根据 pulsar 周期调整 trigger threshold 或预延迟
  // 避免过于密集的触发
  if (periodMs < 50.0f) {  // 高频 pulsar
    triggerThreshold = 0.1f;  // 提高阈值，减少触发
  } else {
    triggerThreshold = 0.01f;  // 低频 infrasonic，更容易触发
  }
}

} // namespace why
