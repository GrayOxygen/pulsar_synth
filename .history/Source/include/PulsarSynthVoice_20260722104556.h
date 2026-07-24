//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "CommonVoiceSate.h"
#include "Commons.h"
#include "DCOffset.h"
#include "PulsarSynthVoice.h"
#include <memory>
/**
 *  pulsar synth voice: The core logic of pulsar is implemented in voice
 */
class PulsarSynthVoice : public juce::SynthesiserVoice {
private:
  // 包络下标
  int envIndex = 0;
  //====================================properties related to Auto  mode==================================== Whether it was in the playing state
  // last time, determine which one is the first time to start playing (rather than after playing).
  bool wasPlayingLastFrame;
  // true: Because the switching mode requires muting, false does not.
  // control the switching mode to pause the sound and allow the user to play
  // again (auto mode)
  bool soundOffWhenSwitchPlayMode;

  //====================================train state====================================
  // loop or one time playback
  bool firstTrain = false;
  bool isActive = false;

  // The duration of the current pulse state (StateEnum) corresponds to the required number of samples
  int currentStateDurationSampleNum = 0;
  // The number of samples processed in the current state (StateEnum)
  int hasPassedSampleNumInsideTrain = 0;
  // The latest stage end position in a single train (unit: samples) is reinitialized each time a new train is entered
  int endPosInTrainSamples = 0.0;

  // The current original stage identifier (before masking, because after  masking)
  why::PulsarStateEnum currentState = why::PulsarStateEnum::IntraSilence;

  // The adsr applied to the original pulsar duty cycle will be applied to the entire pulsar cluster not subdivision if there is a pulsar cluster
  juce::ADSR pulsarAdsr;
  juce::ADSR::Parameters pulsarAdsrParams;

  // Perlin noise for smooth PRF modulation
  why::PerlinNoise perlinNoise;
  double perlinTime = 0.0;
  // Perlin envelope skew: modulates Attack/Decay ratio per grain (1.0 = neutral)
  float envelopeSkew = 1.0f;
  // Whether the release phase has been triggered is used to achieve active
  // release triggering. It is triggered when a specified duration is reached before the end of the current stage, but this judgment is made to avoid repeated triggering
  bool isTriggeredReleaseFlag = false;

  // Count of pulsar stage change times, used to determine which value through the mask whether to release Before mask processing,
  // the train dutycycle is composed of multiple pulsar periods. For example, pulse silence represents 1, 2, 3, 4... 从1开始
  int pulsarStageIndexInTrainDutyCycle = 0;
  // 当前train内已完成的pulsar period数(pulse+silence为一个period)，达到trainDutyCycleLen后进入inter-train silence
  int periodsDoneInTrain = 0;
  // envelope scan across the whole train period (0..2048 mapped to train progress)
  int trainTotalSamples = 0;
  int trainSampleCounter = 0;

  std::atomic<float> currentPulsarModFreq;

  // =====control DAW play stop, train change=====
  std::atomic<bool> enterNextTrain{false};

  //===========================waveform grain pool===========================
  static constexpr int MAX_WAVEFORM_GRAINS = 32;
  struct WaveformGrain {
    double phase = 0.0f; // 一定要用double，不然会累加出误差来
    double phaseInc = 0.0f;
    bool active = false;
    int remainSamples = 0;
    int totalSamples = 0;
    int delaySamples = 0;
    float scale = 0.0f;
    double windowPhase = 0.0f;
    float amp = 0.0f; 
    float wtPos = 0.0f; // wavetable扫描位置(0~1)，spawn时由train相位决定，每个pulsar扫描不同的波形帧
    float wtPosInc = 0.0f; // wtPos每sample的滑动量：grain内连续滑向下一个grain的落点，帧morph无台阶
  };
  WaveformGrain waveformGrains[MAX_WAVEFORM_GRAINS];
  int waveformGrainWriteIdx = 0;
  // 全局LFO相位(voice级)：所有重叠grain在同一时刻共用同一个LFO瞬时值，
  // 保持相位相干(per-grain windowPhase各自不同步查LFO表会让grain间相位漂移而互相抵消)
  double lfoPhase = 0.0;
  double lfoPhaseInc = 0.0;
  float smoothedActiveCount = 1.0f; // 平滑后的活跃grain数，用于避免归一化增益突跳
  //===========================cellular automaton modulation===========================
  // Simple 1D CA (8 cells, Rule 90), for post-pulsar amplitude and pitch offset
  uint8_t caState = 0x01; // single seed cell
  float caPitchOffset = 1.0f;

  //===========================common properties===========================
  std::shared_ptr<CommonVoiceSate> commonVoiceSate;
  SnapShot snapShot;
  DCBlocker dcBlocker;

public:
  PulsarSynthVoice() {}
  //==============================重写方法==============================
  void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int currentPitchWheelPosition) override;

  void stopNote(float /*velocity*/, bool allowTailOff) override;
  void processBlockSamples(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples);

  void renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) override;
  void saveSnapShot();
  void refreshPulsarInSnapShot();
  void renderNextBlockDirectly(juce::AudioSampleBuffer &outputBuffer, juce::AudioPlayHead *audioPlayHead, int startSample, int numSamples);

  void pitchWheelMoved(int) override;

  void controllerMoved(int, int) override;

  bool canPlaySound(juce::SynthesiserSound *sound) override;

  //=============================自定义方法===================================
  int getCellActiveCount();

  /**
   * reset train related status to initial
   */
  void resetTrainInitialSate();

  /**
   * Reset the train according to the specified parameters:
   *
   * In midi mode, startNote will trigger this func
   * In the auto and midi mode, when the train parameter changes, processSample
   * will trigger this func In midi mode, renderNextBlockDirectly will trigger
   * this func during the first playback
   *
   * @param durationLen train duty cycle length
   * @param intervalSilenceLen train interval silence
   * @param isLoop  loop or not
   * @param trainLen  train length
   */
  void resetTrain();

  /**
   * !!!!!!Initializing all train and pulsar related config is equivalent to the overall initialization entry!!!!!!
   *
   * @param sampleRate sample rate
   * @param sampleBuffer sample buffer
   * @param audioPlayHead audio play head to get play position
   */
  void initSynthVoice(double sampleRate, juce::AudioPlayHead *audioPlayHead);

  /**
   * Mapping the parameters maintained by juce to the parameters of synth
   * @param apvts
   */
  void mappingParams(const juce::AudioProcessorValueTreeState &apvts);

  /**
   * Map only a single parameter in the parameterChanged event, because only
   * this value is guaranteed to be up-to-date not apvts.getRawParameterValue
   * (as juce's documentation notice)
   * @param apvts tree state
   * @param parameterID parameter id
   * @param newValue up-to-date value
   */
  void mappingOneParam(const juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue);

  /**
   * When AudioProcessorValueTreeState parameterChanged listening is after the
   * callback, the method will be triggered
   *
   * @param apvts tree state
   * @param parameterID parameter id
   * @param newValue up-to-date value
   * @param isGeneratedStochasticMask has generated this time? true yes, false
   * no
   */
  void parameterChanged(juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue, bool &isGeneratedStochasticMask);

  /**
   * refresh preset to synth voice
   * @param apvts tree state
   */
  void reloadPreset(juce::AudioProcessorValueTreeState &apvts);

  /**
   * refresh adsr
   * @param pulsaretTime: seconds
   */
  void refreshPulsaretAdsr(float pulsaretTime);

  /**
   * whether the current sample has passed the mask
   *
   * @param maskPassFlag The result will be written and updated to this
   * parameter: whether the mask has been passed
   * @param existMask The result will be written and updated to this parameter:
   * whether the mask exist
   */
  void mask(bool &maskPassFlag, bool &existMask);
  bool applyMaskString(const std::string &maskStr, bool &existMask, bool &maskPassFlag);

  /**
   * enter to the next stage
   */
  void changeStage();

  /**
   * calculate new modulated pulsar frequency
   *
   * If the silenceToPulseFlag is false, the pulsar duty cycle is used as the
   * basis for frequency calculation If silenceToPulseFlag is true, the length
   * of pulsar silence is used as the basis for frequency calculation
   *
   * @param pulsarModFreq The new freq/result will be modified into this
   * parameter
   * @param silenceToPulseFlag Was it once silence and now it is pulse
   */
  void calcNewPulsarFreq(float pulsarDutyCycleRatio, float &pulsarModFreq, bool silenceToPulseFlag, int cluster);

  /**
   * process the final samples respectively based on the state
   *
   * @param passMaskFlag passed by mask or not
   * @param existMask mask exist or not
   * @return
   */
  float calSampleByState(bool passMaskFlag, bool existMask);

  /**
   * This method is executed to calculate the sample in both auto and midi modes
   * @return calculated sample and new pulsar frequency
   */
  float processSample();

  /**
   * calculate sample
   *
   * @param pulsarModFreq the newest frequency for sample calculation
   */
  float calcActualPulse();
  void spawnGrain();
  // NuPG式trigger-gating：spawn前用mask决定是否触发本period的grain，被遮蔽则丢弃trigger
  void maskGatedSpawn();
  float getPerlinModulation() const;
  float getCurrentDutyCycleRatio(float phase, float depth);
  float getCurrentDutyCycleCluster(float phase, float depth);
  /**
   * Smoothly transition different waveforms in the waveform table to apply FM
   * modulation with different waveform characteristics, or use fm envelope data
   *
   * @param phase pulsaret phase (0.0 - 1.0) for envelope lookup
   * @param pulsarModFreq new pulsar modulation frequency
   * @param amount decide how much modulation, 0.0f - 1.0f
   * @return modulated frequency offset in semitones
   */
  float calcFmEnvelopeInterpolation(float phase, float pulsarModFreq);
  float calcFmLfoInterpolation(float phase, float pulsarModFreq);
  float calcAmLfoInterpolation(float phase, float pulsarModFreq);

  /**
   * Smoothly transition different waveforms in the waveform table to apply AM
   * modulation with different waveform characteristics, or use am envelope data
   *
   * @param phase pulsaret phase (0.0 - 1.0) for envelope lookup
   * @param amount decide how much modulation, 0.0f - 1.0f
   * @return modulated amplitude value
   */
  float calcAmEnvelopeInterpolation(float phase, float amount);

  /**
   * Get envelope value at given phase using linear interpolation
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return envelope value
   */
  float getAmpEnvelopeValueAtPhase(float phase) const;
  float getFmEnvelopeValueAtPhase(float phase) const;
  float getDutyCycleRatioEnvelopeValueAtPhase(float phase) const;
  float getDutyCycleClusterEnvelopeValueAtPhase(float phase) const;
  float getWaveformEnvelopeValueAtPhase(float phase, float wtPos) const;
  float getFmLfoValueAtPhase(float phase) const;
  float getAmLfoValueAtPhase(float phase) const;
  /**
   * Generic envelope lookup using linear interpolation over ENVELOPE_SIZE points
   * @param data envelope data array
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return interpolated envelope value
   */
  float getEnvelopeValueAtPhase(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data, float phase, float scale = 1.0f) const;

  /**
   * get output gain by converting db value
   * @return output gain
   */
  float getPulseFadeGain() const;

  float getOutputGain();

  [[nodiscard]] std::shared_ptr<CommonVoiceSate> &getCommonVoiceSate() { return commonVoiceSate; }

  void setCommonVoiceSate(std::shared_ptr<CommonVoiceSate> &commonVoiceSate) { this->commonVoiceSate = commonVoiceSate; }
  void setEnterNextTrain(bool flag) { this->enterNextTrain = flag; }
  void setCurrentPulsarDutyCycleFreq(float freq) { this->currentPulsarModFreq = freq; }
  float getCurrentPulsarDutyCycleFreq() { return this->currentPulsarModFreq; }

  void setSoundOffWhenSwitchPlayMode(bool soundOffWhenSwitchPlayMode) { this->soundOffWhenSwitchPlayMode = soundOffWhenSwitchPlayMode; }
};