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
    grains[slot] = {p.startPhase,
                    p.phaseInc,
                    lfoPhase,
                    lfoPhaseInc,
                    true,
                    p.playbackSamples,
                    p.playbackSamples,
                    p.delaySamples,
                    1.0f,
                    0,
                    p.amp,
                    static_cast<float>(p.baseFreq),
                    wtPos,
                    static_cast<float>(wtScanInc)};
    grainWriteIdx = (slot + 1) % MAX_GRAINS;
  }
  // Grain pool management:
  // 1. 优先使用 inactive slot
  // 2. 如果池满，寻找一个“最适合被替换”的 grain
  // 3. 优先替换：接近生命周期末尾 + 当前能量低 + 已经播放较久的 grain
  // 4. 不直接抢占刚出生的 grain，减少 click / discontinuity

  // int slot = -1;
  // // ---------------------------------------------------------
  // // First: find an inactive slot
  // // ---------------------------------------------------------
  // for (int i = 0; i < MAX_GRAINS; ++i) {
  //   const int idx = (grainWriteIdx + i) % MAX_GRAINS;
  //   if (!grains[idx].active) {
  //     slot = idx;
  //     break;
  //   }
  // }

  // // ---------------------------------------------------------
  // // Pool full: find the weakest / safest grain to replace
  // // ---------------------------------------------------------
  // if (slot < 0) {
  //   float bestScore = -std::numeric_limits<float>::max();

  //   for (int i = 0; i < MAX_GRAINS; ++i) {
  //     const int idx = (grainWriteIdx + i) % MAX_GRAINS;
  //     auto &g = grains[idx];

  //     if (!g.active) {
  //       slot = idx;
  //       break;
  //     }

  //     // 生命周期进度：0 = 刚出生，1 = 快结束
  //     const float lifeProgress = g.totalSamples > 0 ? juce::jlimit(0.0f, 1.0f, static_cast<float>(g.totalSamples - g.remainSamples) / static_cast<float>(g.totalSamples)) : 1.0f;

  //     // 越接近结束，越适合被替换
  //     const float nearEnd = lifeProgress * lifeProgress;

  //     // 当前 envelope 越低，越适合被替换
  //     //
  //     // 如果你的 grain 有 envelope phase，
  //     // 可以直接使用当前 envelope value。
  //     // const float envelopePhase = g.totalSamples > 0 ? juce::jlimit(0.0f, 1.0f, static_cast<float>(g.age) / static_cast<float>(g.totalSamples)) : 1.0f;

  //     // const float envelope = std::sin(juce::MathConstants<float>::pi * envelopePhase);

  //     // 防止刚出生的 grain 被马上杀掉
  //     const float ageBonus = lifeProgress;

  //     // 一个非常轻微的随机因素，
  //     // 避免永远按照完全相同的顺序替换
  //     const float randomJitter = juce::Random::getSystemRandom().nextFloat() * 0.05f;

  //     // 越高越应该被替换  + (1.0f - envelope) * 0.25f
  //     const float score = nearEnd * 0.60f + ageBonus * 0.10f + randomJitter * 0.05f;

  //     if (score > bestScore) {
  //       bestScore = score;
  //       slot = idx;
  //     }
  //   }
  // }

  // ---------------------------------------------------------
  // Spawn / replace
  // ---------------------------------------------------------
  if (slot >= 0) {
    grains[slot] = {p.startPhase,
                    p.phaseInc,
                    lfoPhase,
                    lfoPhaseInc,
                    true,
                    p.playbackSamples,
                    p.playbackSamples,
                    p.delaySamples,
                    1.0f,
                    0,
                    p.amp,
                    static_cast<float>(p.baseFreq),
                    wtPos,
                    static_cast<float>(wtScanInc),
                    juce::jlimit(-1.0f, 1.0f, p.pan)};
    grainWriteIdx = (slot + 1) % MAX_GRAINS;
  }
}

void GranularEngine::renderSample(double sampleRate, float &outL, float &outR) {
  double grainSumL = 0.0;
  double grainSumR = 0.0;
  double ampSqSum = 0.0; // 活跃grain的amp²之和(功率)：用于能量归一化
  int activeCount = 0;

  for (auto &g : grains) {
    if (!g.active || g.delaySamples-- > 0 || g.totalSamples <= 0)
      continue;

    activeCount++;
    ampSqSum += static_cast<double>(g.amp) * g.amp;

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
    // am modulation on every pulsaret
    double pulsaretEnvelope = calcAmLfoInterpolation(static_cast<float>(g.phase), commonVoiceSate->amLfoDepthParam->load());
    double adsrWindow = grainAdsrShape(static_cast<float>(g.windowPhase), adsrA, adsrD, adsrS, adsrR);

    g.windowPhase += 1.0 / static_cast<double>(g.totalSamples);
    if (g.windowPhase > 1.0)
      g.windowPhase = 1.0;

    const double contribution = raw * g.amp * pulsaretEnvelope * adsrWindow * completeWindow;
    // per-grain equal-power pan(√2补偿使居中=unity gain，与旧mono路径响度一致)：
    // unison拷贝左右展开时才偏离中心，普通grain pan=0走中心不变
    const double quarterPi = 0.78539816339744831; // pi/4
    const double panAngle = (static_cast<double>(g.pan) + 1.0) * quarterPi;
    grainSumL += contribution * std::cos(panAngle) * 1.4142135623730951;
    grainSumR += contribution * std::sin(panAngle) * 1.4142135623730951;

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
  归一化增益用平滑后的活跃grain功率(amp²之和)而不是grain个数：按个数归一化时，
  低电平的谐波/unison grain与fundamental同权计1，mask造成的不规则生灭让master增益
  大幅wobble(=粗糙/噪感)；按功率归一化，安静grain只贡献amp²，生灭几乎不动增益，
  而N个等幅满电平grain时√(ΣA²)=√N，与旧行为一致。
  一阶低通(~10ms)让增益连续过渡；activeCount==0时冻结，避免下个grain出生时增益从错误值滑动
  if (activeCount > 0) {
    const float targetPower = static_cast<float>(ampSqSum);
    if (targetPower > smoothedGrainPower) {
      // 功率上跳(新grain出生，窗值=0处)立即跟上：mask造成的gap后重新spawn时，
      // 若增益仍按gap期间衰减后的低功率归一化，会瞬间过冲进tanh削波=粗糙/失真
      smoothedGrainPower = targetPower;
    } else {
      // 功率下降(grain死亡)慢速释放(~200ms)：masked period的时长通常只有几十ms，
      // 增益参考在gap期间基本不动——mask本该产生的空隙保持安静，而不是被归一化
      // 重新抬升成增益wobble(=mask速率的AM粗糙感)
      const float alpha = 1.0f - std::exp(-1.0f / (0.2f * static_cast<float>(sampleRate)));
      smoothedGrainPower += (targetPower - smoothedGrainPower) * alpha;
    }
    if (smoothedGrainPower < 1.0f)
      smoothedGrainPower = 1.0f;
  }
  if (activeCount <= 0) {
    outL = 0.0f;
    outR = 0.0f;
    return;
  }
  const double norm = 1.0 / std::sqrt(static_cast<double>(smoothedGrainPower));
  outL = static_cast<float>(grainSumL * norm);
  outR = static_cast<float>(grainSumR * norm);
  // outL = static_cast<float>(grainSumL * 0.9);
  // outR = static_cast<float>(grainSumR * 0.9);
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
