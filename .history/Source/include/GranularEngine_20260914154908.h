//
// GranularEngine: reusable granular core extracted from PulsarSynthVoice.
// It owns the grain pool, voice-level LFO / wavetable-scan state and the
// per-sample overlap-add rendering. The pulsar train logic (PulsarSynthVoice)
// acts as the trigger source: it computes per-grain parameters (amp, pitch,
// duration, drift...) and feeds them to spawn().
//
#pragma once
#include "CommonVoiceSate.h"
#include <memory>

class GranularEngine {
public:
  //===========================waveform grain pool===========================
  // 池容量需 ≥ 最大重叠数×每trigger的spawn路数(基频+谐波+unison最多5路)：
  // OPulWM语义下duty cycle可远超fundamental period(d>p)，重叠数≈durCycles；
  // 池太小时后续pulsar整个被丢弃→发射不规则=亚谐波粗糙感
  static constexpr int MAX_GRAINS = 128;
  struct Grain {
    double phase = 0.0f; // 一定要用double，不然会累加出误差来
    double phaseInc = 0.0f;
    double lfoPhase = 0.0f;
    double lfoPhaseInc = 0.0f;
    bool active = false;
    int remainSamples = 0;
    int totalSamples = 0;
    int delaySamples = 0;
    float scale = 0.0f;
    double windowPhase = 0.0f;
    float amp = 0.0f;
    float fm = 0.0f;
    float wtPos = 0.0f;    // wavetable扫描位置(0~1)，spawn时由train相位决定，每个pulsar扫描不同的波形帧
    float wtPosInc = 0.0f; // wtPos每sample的滑动量：grain内连续滑向下一个grain的落点，帧morph无台阶
    float pan = 0.0f;      // per-grain声像(-1左~+1右，0=居中)：unison拷贝左右展开形成立体声宽度
  };

  // Per-grain parameters computed by the trigger source (pulsar train logic)
  // at trigger time; the engine only handles pool insertion and shared-state latching.
  struct SpawnParams {
    double startPhase = 0.0;
    double phaseInc = 0.0;
    int playbackSamples = 0;
    int delaySamples = 0;
    float amp = 0.0f;
    double baseFreq = 0.0;
    bool fund = true;
    double sampleRate = 44100.0;
    float pan = 0.0f; // -1(左)~+1(右)，0=居中
  };

  void setCommonVoiceSate(const std::shared_ptr<CommonVoiceSate> &state) { commonVoiceSate = state; }

  /**
   * Deactivate all grains and reset their phase (e.g. on transport start),
   * so the first grain of a new train scans the waveform from its start.
   */
  void resetGrains();

  /**
   * Advance the voice-level free-running LFO phase by one sample.
   * spawnGrain在trigger瞬间Latch当前值冻结进grain
   */
  void advanceLfo();
  double getLfoPhase() const { return lfoPhase; }

  /**
   * Insert a grain into the pool (drop-new-keep-old when full) and, for the
   * fundamental, latch the voice-level wavetable-scan / LFO rates.
   */
  void spawn(const SpawnParams &p);

  /**
   * Overlap-add all active grains for one sample and advance the voice-level
   * wavetable scan position (ping-pong). Writes the normalized grain sum as a
   * stereo pair (per-grain equal-power pan, unity gain at center).
   */
  void renderSample(double sampleRate, float &outL, float &outR);

  /**
   * Generic envelope lookup using linear interpolation over ENVELOPE_SIZE points
   * @param data envelope data array
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return interpolated envelope value
   */
  static float getEnvelopeValueAtPhase(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data, float phase, float scale = 1.0f);

private:
  float getWaveformEnvelopeValueAtPhase(float phase, float wtPos, bool perCycleWindow = true) const;
  float calcAmLfoInterpolation(float phase, float depth) const;
  float getAmLfoValueAtPhase(float phase) const;

  Grain grains[MAX_GRAINS];
  int grainWriteIdx = 0;
  // 全局LFO相位(voice级)：所有重叠grain在同一时刻共用同一个LFO瞬时值，
  // 保持相位相干(per-grain windowPhase各自不同步查LFO表会让grain间相位漂移而互相抵消)
  double lfoPhase = 0.0;
  double lfoPhaseInc = 1.0;
  // 全局wavetable扫描位置(voice级，自由运行)：所有重叠grain每sample共用同一个扫描位置，
  // 帧morph逐sample连续滑动且grain间完全相干——不会出现per-grain冻结位置导致的台阶/合唱感；
  // 边界ping-pong反弹(0~1来回)，避免wrap(末帧->首帧)的音色突跳
  double wtScanPos = 0.0;
  double wtScanInc = 0.0;

  std::shared_ptr<CommonVoiceSate> commonVoiceSate;
};
