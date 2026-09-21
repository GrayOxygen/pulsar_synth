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
  if (p.fund) {
    // wtScanInc = (wtScanInc < 0.0 ? -1.0 : 1.0) * p.baseFreq / (EnvelopeCanvas::ENVELOPE_SIZE * p.sampleRate);
    wtScanInc = (wtScanInc < 0.0 ? -1.0 : 1.0) * p.baseFreq / (p.sampleRate);
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
  int activeCount = 0;

  for (auto &g : grains) {
    if (!g.active || g.delaySamples-- > 0 || g.totalSamples <= 0)
      continue;

    activeCount++;

    // 经典granular平滑的核心是COLA(constant overlap-add)：整段Hann窗+重叠≥2时，
    // 所有窗的和恒为常数——grain之间纯crossfade、零波纹，生灭完全听不见；
    // Tukey平顶窗不满足COLA，每次生灭都留台阶残余(=颗粒噪/crack)。
    // overlap用totalSamples×phaseInc(=时长折算成周期数，trigger每周期一次)现场推算：
    // ≥2个周期用Hann(COLA成立)，<2(无重叠，纯pulsaret)退回Tukey平顶保留冲击感
    float overlapCycles = static_cast<float>(g.totalSamples * g.phaseInc);

    // Roads OPulWM语义：多周期duty cycle的pulsaret是"一个"pulsaret，包络作用于整个
    // duty cycle(整段grain窗)，而不是每个波形周期。之前的per-cycle hann把长grain内
    // 每个周期都斩成离散脉冲——重叠拷贝的斩波相位互不对齐(frac(T·fd)错开)，叠加包络
    // 变成f_d×N速率的深浅不一的鼓包=颗粒粗糙感。重叠(≥2周期)时关掉逐周期hann，
    // 只留5%平滑edge fade(=Roads的edge factor，消wrap不连续)——重叠区变成干净的
    // 相位抵消/梳状滤波(OPulWM本来的subtle效果)；单周期pulsaret保留hann(pulsaret本体窗)
    const bool perCycleWindow = overlapCycles < 2.0f;
    // 所有grain读同一个voice级扫描位置：重叠grain帧位置完全相干，crossfade平滑无台阶
    double raw = getWaveformEnvelopeValueAtPhase(static_cast<float>(g.phase), static_cast<float>(wtScanPos), perCycleWindow);
    raw = juce::jlimit(-1.0, 1.0, raw);

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
    double pulsaretEnvelope = calcAmLfoInterpolation(static_cast<float>(g.windowPhase), commonVoiceSate->amLfoDepthParam->load());
    double adsrWindow = grainAdsrShape(static_cast<float>(g.phase), adsrA, adsrD, adsrS, adsrR);

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
  wtScanPos += wtScanInc * activeCount;
  // ping-pong反弹代替wrap：wrap(末帧->首帧)的音色突变每扫完一圈crack一次，
  // 反弹让morph轨迹连续来回、无接缝
  if (wtScanPos >= 1.0) {
    wtScanPos = 2.0 - wtScanPos;
    wtScanInc = -wtScanInc;
  } else if (wtScanPos < 0.0) {
    wtScanPos = -wtScanPos;
    wtScanInc = -wtScanInc;
  }
  // 无运行时master归一化：重叠补偿已在spawn时按预期重叠数烘焙进每颗grain的amp
  // (spawnGrain里amp/=overlapEst)。任何按当前活跃grain数/功率跟踪的master增益，
  // 在mask的不规则生灭下都会随pool状态跳变——增益台阶作用在整个正在响的mix上，
  // 就是mask模式下每个边界"不平滑/咔"的来源。烘焙式补偿下生灭只影响该grain自身，
  // mask空洞表现为COLA窗和的平滑凹陷(=mask应有的听感)，总线增益恒定零调制
  if (activeCount <= 0) {
    outL = 0.0f;
    outR = 0.0f;
    return;
  }
  outL = static_cast<float>(grainSumL);
  outR = static_cast<float>(grainSumR);
}

float GranularEngine::getWaveformEnvelopeValueAtPhase(float phase, float wtPos, bool perCycleWindow) const {
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
  // (=Roads的edge factor：duty cycle边界处的用户可控crossfade)
  constexpr float fadeRatio = 0.05f;
  float p = phase - std::floor(phase);
  if (p < fadeRatio) {
    float t = p / fadeRatio;
    v *= t * t * (3.0f - 2.0f * t); // smoothstep fade in
  } else if (p > 1.0f - fadeRatio) {
    float t = (1.0f - p) / fadeRatio;
    v *= t * t * (3.0f - 2.0f * t); // smoothstep fade out
  }

  // 逐周期hann=单周期pulsaret的本体窗；多周期duty cycle(重叠/OPulWM)时由整段grain窗
  // 负责包络，这里不再把每个周期斩成脉冲(否则重叠拷贝的斩波鼓包互相错位=颗粒粗糙感)
  return perCycleWindow ? v * hannWindow(phase) : v;
}

float GranularEngine::calcAmLfoInterpolation(float phase, float depth) const {
  return (0.5f + (2.0f * getAmLfoValueAtPhase(phase) * depth - 1.0f) / 2.0f); // [0, 1]
}

float GranularEngine::getAmLfoValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->amLfoData, phase, commonVoiceSate->amLfoScale.load()); }
