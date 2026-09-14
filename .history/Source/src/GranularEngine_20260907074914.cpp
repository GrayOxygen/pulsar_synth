//
// GranularEngine: granular core processing extracted from PulsarSynthVoice.
//
#include "../include/GranularEngine.h"
#include <algorithm>
#include <cmath>

namespace {
inline float hannWindow(float phase) {
  phase = juce::jlimit(0.0f, 1.0f, phase);
  return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));
}

// 每个波形周期的边缘淡入淡出窗(Tukey)：只在周期首尾fade%做余弦淡化消除wrap不连续的click，
// 中段保持1.0——不像整周期hann那样每个周期把grain掐到0，overlap求和不再在基频处深度波动
inline float cycleEdgeWindow(float phase, float fade = 0.05f) {
  phase = juce::jlimit(0.0f, 1.0f, phase);
  if (phase < fade)
    return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * phase / fade));
  if (phase > 1.0f - fade)
    return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (1.0f - phase) / fade));
  return 1.0f;
}

// per-grain ADSR形状(包络作用范围=单个grain完整寿命)：t为grain窗相位0~1，
// a/d/r为占寿命的归一化比例(a+d+r≤1)，s为sustain电平；首尾必为0，grain生灭无click
inline float grainAdsrShape(float t, float a, float d, float s, float r) {
  if (t <= 0.0f || t >= 1.0f)
    return 0.0f;
  if (t < a)
    return t / a;
  if (d > 0.0f && t < a + d)
    return 1.0f + (s - 1.0f) * ((t - a) / d);
  float relStart = 1.0f - r;
  if (t < relStart)
    return s;
  return s * (1.0f - (t - relStart) / r);
}
} // namespace

void GranularEngine::resetGrains() {
  for (auto &g : grains) {
    g.active = false;
    g.phase = 0.0; // 重置相位，保证新train的第一个grain从波形起点开始扫描
  }
  grainWriteIdx = 0;
}

void GranularEngine::advanceLfo() {
  lfoPhase += lfoPhaseInc;
  if (lfoPhase >= 1.0)
    lfoPhase -= 1.0;
}

void GranularEngine::spawn(const SpawnParams &p) {
  // 只在fundamental spawn时Latch voice级速率：spawnGrain每trigger被调用4次(基频+2/3/4次谐波)，
  // 不加fund门限时wtScanInc/lfoPhaseInc被最后一个谐波用4×baseFreq覆盖，
  // 且谐波spawn被跳过(length≤0)时又回落——扫描/LFO速率在1×~4×之间每trigger乱跳，
  // 就是听到的噪/crack的主源之一
  if (p.fund) {
    // wavetable scanning: one full table sweep per 2048 pulsar periods (matches envPhase timeline),
    // direction sign preserved so ping-pong isn't reset
    wtScanInc = (wtScanInc < 0.0 ? -1.0 : 1.0) * p.baseFreq / (2048.0 * p.sampleRate);
    // follow the scan freq, latch：phaseInc已含调制，不再额外乘
    lfoPhaseInc = p.phaseInc;
  }
  float wtPos = static_cast<float>(wtScanPos);

  // 池满时丢弃新grain(丢新保旧)，避免高重叠时的硬切click
  int slot = -1;
  for (int i = 0; i < MAX_GRAINS; ++i) {
    int idx = (grainWriteIdx + i) % MAX_GRAINS;
    if (!grains[idx].active) {
      slot = idx;
      break;
    }
  }
  if (slot >= 0) {
    grains[slot] = {p.startPhase, p.phaseInc, lfoPhase, lfoPhaseInc, true, p.playbackSamples, p.playbackSamples, p.delaySamples, 1.0f, 0, p.amp, static_cast<float>(p.baseFreq), wtPos,
                    static_cast<float>(wtScanInc)};
    grainWriteIdx = (slot + 1) % MAX_GRAINS;
  }
}

float GranularEngine::renderSample(double sampleRate) {
  double grainSum = 0.0;
  int activeCount = 0;

  for (auto &g : grains) {
    if (!g.active || g.delaySamples-- > 0 || g.totalSamples <= 0)
      continue;

    activeCount++;

    // 所有grain读同一个voice级扫描位置：重叠grain帧位置完全相干，crossfade平滑无台阶
    double raw = getWaveformEnvelopeValueAtPhase(static_cast<float>(g.phase), static_cast<float>(wtScanPos));
    raw = juce::jlimit(-1.0, 1.0, raw);

    // 经典granular平滑的核心是COLA(constant overlap-add)：整段Hann窗+重叠≥2时，
    // 所有窗的和恒为常数——grain之间纯crossfade、零波纹，生灭完全听不见；
    // Tukey平顶窗不满足COLA，每次生灭都留台阶残余(=颗粒噪/crack)。
    // overlap用totalSamples×phaseInc(=时长折算成周期数，trigger每周期一次)现场推算：
    // ≥2个周期用Hann(COLA成立)，<2(无重叠，纯pulsaret)退回Tukey平顶保留冲击感
    float overlapCycles = static_cast<float>(g.totalSamples * g.phaseInc);
    double completeWindow = overlapCycles >= 2.0f ? 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * g.windowPhase) : cycleEdgeWindow(static_cast<float>(g.windowPhase));
    // 周期内只做首尾短淡化(声明wrap处防click)，不再整周期hann斩波——overlap叠加更平滑
    // a+d+r>1时按比例压缩，保证形状始终合法
    float adsrA = commonVoiceSate->grainAdsrAttack.load();
    float adsrD = commonVoiceSate->grainAdsrDecay.load();
    float adsrS = commonVoiceSate->grainAdsrSustain.load();
    float adsrR = commonVoiceSate->grainAdsrRelease.load();
    float adsrSum = adsrA + adsrD + adsrR;
    if (adsrSum > 1.0f) {
      adsrA /= adsrSum;
      adsrD /= adsrSum;
      adsrR /= adsrSum;
    }
    
    double pulsaretEnvelope = calcAmLfoInterpolation(static_cast<float>(g.windowPhase), commonVoiceSate->amLfoDepthParam->load());
    double adsrWindow = grainAdsrShape(static_cast<float>(g.windowPhase), adsrA, adsrD, adsrS, adsrR);

    g.windowPhase += 1.0 / static_cast<double>(g.totalSamples);
    if (g.windowPhase > 1.0)
      g.windowPhase = 1.0;

    grainSum += raw * g.amp * pulsaretEnvelope * adsrWindow * completeWindow;

    g.phase += g.phaseInc;
    if (g.phase >= 1.0f) {
      g.phase -= 1;
    }

    g.remainSamples--;
    if (g.remainSamples <= 0) {
      g.active = false;
    }
  }
  // voice级扫描位置逐sample推进；ping-pong反弹代替wrap：到达边界时往回滑而不是从末帧跳回首帧，
  // wrap处的音色突变会产生毛刺——反弹让morph轨迹连续来回扫描
  wtScanPos += wtScanInc;
  // ping-pong反弹代替wrap：wrap(末帧->首帧)的音色突变每扫完一圈crack一次，
  // 反弹让morph轨迹连续来回、无接缝
  if (wtScanPos >= 1.0) {
    wtScanPos = 2.0 - wtScanPos;
    wtScanInc = -wtScanInc;
  } else if (wtScanPos < 0.0) {
    wtScanPos = -wtScanPos;
    wtScanInc = -wtScanInc;
  }
  // 归一化增益用平滑后的活跃grain数：瞬时activeCount在grain生灭时阶跃，每次阶跃都把全部grain
  // 电平突跳一次(台阶信号=发射率及以下的低频thump)；一阶低通(~10ms)让增益连续过渡，
  // activeCount==0时冻结，避免下个grain出生时增益从错误值滑动
  if (activeCount > 0) {
    const float alpha = 1.0f - std::exp(-1.0f / (0.01f * static_cast<float>(sampleRate)));
    smoothedActiveCount += (static_cast<float>(activeCount) - smoothedActiveCount) * alpha;
    if (smoothedActiveCount < 1.0f)
      smoothedActiveCount = 1.0f;
  }
  return activeCount <= 0 ? 0.0f : static_cast<float>(grainSum / std::sqrt(static_cast<double>(smoothedActiveCount)));
}

float GranularEngine::getEnvelopeValueAtPhase(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data, float phase, float scale) {
  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  phase = std::fmod(std::abs(phase), 1.0f);
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;
  float val1 = data[std::min(index, size - 1)] * scale;
  float val2 = data[std::min(index + 1, size - 1)] * scale;
  return val1 + frac * (val2 - val1);
}

float GranularEngine::getWaveformEnvelopeValueAtPhase(float phase, float wtPos) const {
  const float scale = commonVoiceSate->pgWaveformEnvelopeScale.load();
  const int numFrames = commonVoiceSate->pgWavetableNumFrames.load();

  // wavetable scanning：wtPos(0~1)映射到帧序列，相邻两帧线性morph；单帧时退化为原来的单波形查表
  float v, dc;
  if (numFrames <= 1) {
    v = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[0], phase, scale);
    dc = commonVoiceSate->pgWavetableDcOffset[0].load();
  } else {
    float fpos = juce::jlimit(0.0f, 1.0f, wtPos) * static_cast<float>(numFrames - 1);
    int f0 = static_cast<int>(fpos);
    int f1 = std::min(f0 + 1, numFrames - 1);
    float t = fpos - static_cast<float>(f0);
    float v0 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[f0], phase, scale);
    float v1 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[f1], phase, scale);
    v = v0 * (1.0f - t) + v1 * t;
    dc = commonVoiceSate->pgWavetableDcOffset[f0].load() * (1.0f - t) + commonVoiceSate->pgWavetableDcOffset[f1].load() * t;

    // float fpos = juce::jlimit(0.0f, 1.0f, wtPos) * static_cast<float>(numFrames - 1);
    // int f0 = static_cast<int>(fpos);
    // if (f0 != currentFrame) {
    //   currentFrame = nextFrame;
    //   nextFrame = juce::Random::getSystemRandom().nextInt(numFrames);
    //   fpos = currentFrame;
    //   f0 = currentFrame;
    // }
    // int f1 = std::min(f0 + 1, numFrames - 1);
    // nextFrame = f1;
    // float t = fpos - static_cast<float>(f0);
    // float v0 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[f0], phase, scale);
    // float v1 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[f1], phase, scale);
    // v = v0 * (1.0f - t) + v1 * t;
    // dc = commonVoiceSate->pgWavetableDcOffset[f0].load() * (1.0f - t) + commonVoiceSate->pgWavetableDcOffset[f1].load() * t;

    // float t = juce::jlimit(0.0f, 1.0f, wtPos);
    // float v0 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[currentFrame], phase, scale);
    // float v1 = getEnvelopeValueAtPhase(commonVoiceSate->pgWavetable[nextFrame], phase, scale);
    // float dc0 = commonVoiceSate->pgWavetableDcOffset[currentFrame].load();
    // float dc1 = commonVoiceSate->pgWavetableDcOffset[nextFrame].load();
    // v = v0 * (1.0f - t) + v1 * t;
    // dc = dc0 * (1.0f - t) + dc1 * t;
    // if (wtPos < lastWtPos) {
    //   currentFrame = nextFrame;

    //   do {
    //   } while (numFrames > 1 && nextFrame == currentFrame);
    // }

    // lastWtPos = wtPos;
  }

  // DC源头消除：减去波形的hann加权均值，使每个grain(波形×hann窗)积分为0，pulse train不再携带DC offset
  v -= dc * scale;

  // 每个波形周期做fade in/out：首尾各fadeRatio区间幅度平滑归零，wrap(1->0)处必然过零，任意波形都不会跳变
  constexpr float fadeRatio = 0.05f;
  float p = phase - std::floor(phase);
  if (p < fadeRatio) {
    float t = p / fadeRatio;
    v *= t * t * (3.0f - 2.0f * t); // smoothstep fade in
  } else if (p > 1.0f - fadeRatio) {
    float t = (1.0f - p) / fadeRatio;
    v *= t * t * (3.0f - 2.0f * t); // smoothstep fade out
  }

  return v * hannWindow(phase);
}

float GranularEngine::calcAmLfoInterpolation(float phase, float depth) const {
  return (0.5f + (2.0f * getAmLfoValueAtPhase(phase) * depth - 1.0f) / 2.0f); // [0, 1]
}

float GranularEngine::getAmLfoValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->amLfoData, phase, commonVoiceSate->amLfoScale.load()); }
