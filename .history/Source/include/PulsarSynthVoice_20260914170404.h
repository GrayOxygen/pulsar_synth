//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "CommonVoiceSate.h"
#include "Commons.h"
#include "DCOffset.h"
#include "GranularEngine.h"
#include "PulsarSynthVoice.h"
#include <memory>
/**
 *  pulsar synth voice: The core logic of pulsar is implemented in voice
 */
class PulsarSynthVoice : public juce::SynthesiserVoice {
private:
  // 包络下标
  int envIndex = 0;
  // BPM包络下标(latch方式)：不随envIndex推进，每个stage走完一次，推进到下一个点并latch新bpm
  int bpmEnvIndex = 0;
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

  // Perlin noise for smooth PRF modulation
  why::PerlinNoise perlinNoise;
  double perlinTime = 0.0;
  // Perlin envelope skew: modulates Attack/Decay ratio per grain (1.0 = neutral)
  float envelopeSkew = 1.0f;
  // Whether the release phase has been triggered is used to achieve active
  // release triggering. It is triggered when a specified duration is reached before the end of the current stage, but this judgment is made to avoid repeated triggering
  bool isTriggeredReleaseFlag = false;

  // @deprecated
  // Count of pulsar stage change times, used to determine which value through the mask whether to release Before mask processing,
  // the train dutycycle is composed of multiple pulsar periods. For example, pulse silence represents 1, 2, 3, 4... 从1开始
  int pulsarStageIndexInTrainDutyCycle = 0;
  // 当前train内已完成的pulsar period数(pulse+silence为一个period)，达到trainDutyCycleLen后进入inter-train silence
  int periodsDoneInTrain = 0;
  long periodsDoneCounter = 0;
  // envelope scan across the whole train period (0..96 mapped to train progress)
  int trainTotalSamples = 0;
  int trainSampleCounter = 0;

  std::atomic<float> currentPulsarModFreq;

  // =====control DAW play stop, train change=====
  std::atomic<bool> enterNextTrain{false};

  //===========================granular core===========================
  // 可复用的granular核心引擎：grain池、voice级LFO/波表扫描状态、overlap-add渲染都在其中；
  // pulsar train逻辑(本类)作为trigger源，在每个pulsar period边界计算参数并spawn grain
  GranularEngine granular;
  float smoothedFmRatio = 1.0f; // 平滑后的FM LFO音高ratio：相邻grain间滑动而不是台阶跳变
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

  // nextframe is decided by random number
  int currentFrame = 0;
  int nextFrame = 0;
  float lastWtPos = 0.0f;

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
   * latch当前bpm包络点的值（限幅到合理BPM范围）
   */
  float getCurrentBpm() const;

  /**
   * latch方式推进bpm包络：每个stage走完一次调用，推进到下一个点并刷新快照中bpm相关的train时长
   */
  void advanceBpmEnvelope();

  /**
   * 用最新latch的bpm重算快照中的train时长相关字段
   */
  void refreshBpmInSnapShot();

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
   * process the final samples respectively based on the state
   *
   * @param passMaskFlag passed by mask or not
   * @param existMask mask exist or not
   */
  void calSampleByState(bool passMaskFlag, bool existMask, float &outL, float &outR);

  /**
   * This method is executed to calculate the sample in both auto and midi modes,
   * writes the stereo pair (per-grain pan from the granular engine)
   */
  void processSample(float &outL, float &outR);

  /**
   * calculate sample (stereo overlap-add from the granular engine)
   */
  void calcActualPulse(float &outL, float &outR);
  void spawnGrain(double baseFreq, double amFactor = 1.0f, double length = 1.0f, bool fund = true, double rateMod = 1.0, double extraDetuneCents = 0.0, float pan = 0.0f);
  // trigger-gating：spawn前用mask决定是否触发本period的grain，被遮蔽则丢弃trigger
  void maskGatedSpawn();
  float getPerlinModulation() const;
  float getCurrentPan(float phase, float depth);
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
  float getPanEnvelopeValueAtPhase(float phase) const;
  float getDutyCycleClusterEnvelopeValueAtPhase(float phase) const;
  float getFmLfoValueAtPhase(float phase) const;
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

  void setCommonVoiceSate(std::shared_ptr<CommonVoiceSate> &commonVoiceSate) {
    this->commonVoiceSate = commonVoiceSate;
    granular.setCommonVoiceSate(commonVoiceSate);
  }
  void setEnterNextTrain(bool flag) { this->enterNextTrain = flag; }
  void setCurrentPulsarDutyCycleFreq(float freq) { this->currentPulsarModFreq = freq; }
  float getCurrentPulsarDutyCycleFreq() { return this->currentPulsarModFreq; }

  void setSoundOffWhenSwitchPlayMode(bool soundOffWhenSwitchPlayMode) { this->soundOffWhenSwitchPlayMode = soundOffWhenSwitchPlayMode; }
};