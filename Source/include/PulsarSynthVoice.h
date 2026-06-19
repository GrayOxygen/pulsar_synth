//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "CommonVoiceSate.h"
#include "Commons.h"
#include "LfoModulator.h"
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
  // The number of times the train has been completed is used to determine the
  // non-loop mode. If it is played once, it will end. For now, it can only be
  // played in a loop
  int trainCounter = 0;
  bool isActive = false;

  int samplesInPulsar = 0.0;
  // The duration of the current pulse state (StateEnum) corresponds to the required number of samples
  int currentStateDurationSampleNum = 0.0;
  // The number of samples processed in the current state (StateEnum), -999 indicates the start of the train
  int hasPassedSampleNumInsideTrain = -999.0f;
  // The latest position in a single train (unit: samples) is reinitialized each time a new train is entered
  int endPosInTrainSamples = 0.0;

  // The current original stage identifier (before masking, because after  masking)
  why::PulsarStateEnum currentState = why::PulsarStateEnum::IntraSilence;

  // The adsr applied to the original pulsar duty cycle will be applied to the entire pulsar cluster not subdivision if there is a pulsar cluster
  juce::ADSR pulsarAdsr;
  juce::ADSR::Parameters pulsarAdsrParams;
  // Whether the release phase has been triggered is used to achieve active
  // release triggering. It is triggered when a specified duration is reached before the end of the current stage, but this judgment is made to avoid repeated triggering
  bool isTriggeredReleaseFlag = false;

  // Count of pulsar stage change times, used to determine which value through the mask whether to release Before mask processing,
  // the train dutycycle is composed of multiple pulsar periods. For example, pulse silence represents 1, 2, 3, 4...
  int pulsarStageIndexInTrainDutyCycle = 0;
  // pulsaret phase
  float pulsaretPhase = 0.0f;
  // per-voice LFO modulator (independent phase per voice/instance)
  LfoModulator lfoModulator;

  // The default time of 1 beat (in seconds)
  // double trainLenBlock;
  // // trainTime = trainLen * trainLenBlock;
  // double trainTime;

  // //========train duty cycle, train silence and train len all lead to changes in train. bpm is the same======== The length of the last train duty cycle: How
  // // many pulsar periods
  // int previousTrainDutyCycleNum = 0;
  // // The length of the last train silence: How many pulsar periods
  // int previousTrainSilenceNum = 0;
  // // The length of the last train: How many trainLenBlocks(beats)
  // int previousTrainLen = 0;

  // // train period time = trainDutyCycleTime + trainSilenceTime，sec
  // float trainPeriodTime = 0.0f;
  // // train duty cycle: The actual duration of the sent pulse period is in
  // // seconds
  // float trainDutyCycleTime = 0.0f;
  // // After the train duty cycle is over, enter the train silence in seconds
  // float trainSilenceTime = 0.0f;

  // =====control DAW play stop, train change=====
  // Whether it is necessary to enter the new train or not, each train change
  // will not directly rebuild the train. Wait until the nearest pulsar silence
  // or train silence ends before entering. If all silence are 0, proceed to the
  // next train after the most recent pulse ends
  std::atomic<bool> changeTrainTrace{false};
  // // true: The same train. false: If the parameters related to train change, change train trace can be allowed
  // bool isTheSameTrainConfig;
  // // new train duration length
  // int newTrainDurationLen;
  // // new train intervale silence length
  // int newTrainIntervalSilenceLen;
  // // new train len
  // int newTrainLen;
  // // pulsar samples = pulsar duty cycle time (if cluster>0，this is pulsar duty cycle, also all divisions sum too) * sampleRate
  // int pulsarDutyCycleSamples;
  // // pulsar silence time * sample rate
  // int pulsarIntraSilenceSamples;
  // // train interval silence time * sample rate
  // int interTrainSilenceSamples;
  // // train duty cycle time * sample rate
  // int trainDutyCycleSamples;

  //===========================pulsaret===========================
  // // fundamental frequency of pulsar emitter，This actually not used much because this plugin mainly focuses on train
  // float fundamentalFreq = 0.0;
  // // ratio = pulse duty cycle length/(pulse duty cycle length + silence length)
  // float pulsarDutyCycleRatio = 0.5f;
  // // pulsar period = pulsar duty cycle time + pulsar silence time
  // float pulsarPeriodTime = 0.0f;
  // // pulsar silence
  // float pulsarSilenceTime = 0.0f;

  //===========================pulsaret envelope===========================

  // cache the sample rate last applied to pulsarAdsr to avoid redundant
  // setSampleRate on the audio thread
  // double adsrSampleRate = 0.0;

  //===========================waveform grain pool===========================
  static constexpr int MAX_WAVEFORM_GRAINS = 64;
  struct WaveformGrain {
    float phase = 0.0f;
    float phaseInc = 0.0f;
    bool active = false;
  };
  WaveformGrain waveformGrains[MAX_WAVEFORM_GRAINS];
  int waveformGrainWriteIdx = 0;

  //===========================common properties===========================
  std::shared_ptr<CommonVoiceSate> commonVoiceSate;
  SnapShot snapShot;

public:
  PulsarSynthVoice() {}
  //==============================重写方法==============================
  void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int currentPitchWheelPosition) override;

  void stopNote(float /*velocity*/, bool allowTailOff) override;
  void processSampleWithConvolution(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples);

  void renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) override;
  void saveSnapShot();
  void refreshSnapShot(int trainDurationLen, int trainIntervalSilenceLen, int trainLen);

  void renderNextBlockDirectly(juce::AudioSampleBuffer &outputBuffer, juce::AudioPlayHead *audioPlayHead, int startSample, int numSamples);

  void pitchWheelMoved(int) override;

  void controllerMoved(int, int) override;

  bool canPlaySound(juce::SynthesiserSound *sound) override;

  //=============================自定义方法===================================
  /**
   * calculate pulsar silence time
   * @param dutyCylceRatio pulsar duty cycle ratio =  pulsar duty cycle/pulsar
   * period
   */
  void setPulsarSilence(float dutyCylceRatio);

  /**
   *If the train-related parameters indicate that the train has changed, wait
   * for the next pulsar silence or train silence to end and enter the new
   * train. If all silos are 0, then after the most recent pulse ends, enter a
   * new train
   *
   * @param bpmChangedFlag is bpm changed or not
   */
  void changeToNewTrainAfterPulsarPeriodOrTrainEnd(bool bpmChangedFlag);

  /**
   * Update the latest train config directly to synth without any delay
   */
  void realChangeTrainConfig();

  /**
   * init train
   *
   * @param durationLen train duty cycle length
   * @param intervalSilenceLen train interval silence length
   * @param isLoop loop or not
   * @param trainLen train length
   */
  void initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);

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
  void resetTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);

  /**
   * !!!!!!Initializing all train and pulsar related config is equivalent to the overall initialization entry!!!!!!
   *
   * @param sampleRate sample rate
   * @param sampleBuffer sample buffer
   * @param audioPlayHead audio play head to get play position
   */
  void initSynthVoice(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>> &sampleBuffer, juce::AudioPlayHead *audioPlayHead);

  /**
   * init audip parameter to synth voice
   * @param apvts
   */
  void connectParameters(juce::AudioProcessorValueTreeState &apvts);

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
   * @param pulsarDutyCycleTime
   */
  void refreshPulsaretAdsr(float pulsarDutyCycleTime);

  /**
   * whether the current sample has passed the mask
   *
   * @param maskPassFlag The result will be written and updated to this
   * parameter: whether the mask has been passed
   * @param existMask The result will be written and updated to this parameter:
   * whether the mask exist
   */
  void mask(bool &maskPassFlag, bool &existMask);

  /**
   * enter to the next stage
   *
   * @param pulsarDutyCycleSamples current pulsar duty cycle samples
   * @param intraSilenceSamples  current pulsar silence samples
   * @param interTrainSilenceSamples current train interval silence samples
   * @param trainDutyCycleSamples  current train duty cycle samples
   */
  void changeStage(int pulsarDutyCycleSamples, int intraSilenceSamples, int interTrainSilenceSamples, int trainDutyCycleSamples);

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
   * @param pulsarModFreq modulate frequency
   * @return
   */
  float calSampleByState(bool passMaskFlag, bool existMask, float pulsarModFreq);

  /**
   * When reset train, recalculate the relevant samples to locate the play
   * position info
   */
  void resetTrainRelatedSamples4Location();

  /**
   * This method is executed to calculate the sample in both auto and midi modes
   * @return calculated sample and new pulsar frequency
   */
  std::pair<float, float> processSample();

  /**
   * calculate sample
   *
   * @param pulsarModFreq the newest frequency for sample calculation
   * @return sample
   */
  float calcActualPulse(float pulsarModFreq);
  float getCurrentDutyCycleRatio(float phase, float depth);
  float getCurrentDutyCycleCluster(float phase, float depth);
  float getCurrentSampleFromWaveformEnvelope(float phase);
  /**
   * Smoothly transition different waveforms in the waveform table to apply FM
   * modulation with different waveform characteristics, or use fm envelope data
   *
   * @param phase pulsaret phase (0.0 - 1.0) for envelope lookup
   * @param pulsarModFreq new pulsar modulation frequency
   * @param amount decide how much modulation, 0.0f - 1.0f
   * @return modulated frequency offset in semitones
   */
  float calcFormantLfoInterpolation(float phase, float pulsarModFreq);

  /**
   * Smoothly transition different waveforms in the waveform table to apply AM
   * modulation with different waveform characteristics, or use am envelope data
   *
   * @param phase pulsaret phase (0.0 - 1.0) for envelope lookup
   * @param amount decide how much modulation, 0.0f - 1.0f
   * @return modulated amplitude value
   */
  float calcAmpLfoInterpolation(float phase, float amount);

  /**
   * Get envelope value at given phase using linear interpolation
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return envelope value
   */
  float getAmpEnvelopeValueAtPhase(float phase) const;

  /**
   * Get FM envelope value at given phase using linear interpolation
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return envelope value (semitones offset)
   */
  float getFmEnvelopeValueAtPhase(float phase) const;
  float getDutyCycleRatioEnvelopeValueAtPhase(float phase) const;
  float getDutyCycleClusterEnvelopeValueAtPhase(float phase) const;
  float getWaveformEnvelopeValueAtPhase(float phase) const;

  /**
   * Get Cluster envelope value at given phase using linear interpolation
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return envelope value (cluster multiplier)
   */
  float getClusterEnvelopeValueAtPhase(float phase) const;

  /**
   * Get Duty Ratio envelope value at given phase using linear interpolation
   * @param phase pulsaret phase (0.0 - 1.0)
   * @return envelope value (duty ratio 0.0 - 1.0)
   */
  float getDutyRatioEnvelopeValueAtPhase(float phase) const;

  /**
   * get output gain by converting db value
   * @return output gain
   */
  float getOutputGain();

  /**
   * Just update bpm
   * @param bpm new bpm
   * @return true: changed a different bpm false:no need to update
   */
  bool updateBpmDirectly(float bpm);

  [[nodiscard]] std::shared_ptr<CommonVoiceSate> &getCommonVoiceSate() { return commonVoiceSate; }

  void setCommonVoiceSate(std::shared_ptr<CommonVoiceSate> &commonVoiceSate) { this->commonVoiceSate = commonVoiceSate; }
  void setChangeTrainTrace(bool flag) { this->changeTrainTrace = flag; }

  void setSoundOffWhenSwitchPlayMode(bool soundOffWhenSwitchPlayMode) { this->soundOffWhenSwitchPlayMode = soundOffWhenSwitchPlayMode; }
};