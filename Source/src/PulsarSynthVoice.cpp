//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/PulsarSynthVoice.h"
#include "../include/CommonVoiceSate.h"
#include "../include/LfoModulator.h"
#include "../include/PulsarSynthSound.h"
#include "../include/PulsaretWaveformSingleton.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>

//================================================ overwrite methods ================================================
// These juce::SynthesiserVoice overrides are required by the base class but are unused:
// this plugin is driven by the DAW transport (Auto mode) via renderNextBlockDirectly, not by MIDI notes.
void PulsarSynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int currentPitchWheelPosition) {
  juce::ignoreUnused(midiNoteNumber, velocity, sound, currentPitchWheelPosition);
}

void PulsarSynthVoice::stopNote(float, bool allowTailOff) {
  juce::ignoreUnused(allowTailOff);
  clearCurrentNote();
}

void PulsarSynthVoice::pitchWheelMoved(int) {}

void PulsarSynthVoice::controllerMoved(int, int) {}

bool PulsarSynthVoice::canPlaySound(juce::SynthesiserSound *sound) { return dynamic_cast<PulsarSynthSound *>(sound) != nullptr; }

void PulsarSynthVoice::renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) { juce::ignoreUnused(outputBuffer, startSample, numSamples); }

// ================================== customize method ：train相关参数不要load，每次输出时，只使用snapshot，保证前后计算统一不出错 ====================================
void PulsarSynthVoice::saveSnapShot() {
  snapShot.bpm = commonVoiceSate->bpm->load();
  snapShot.trainDutyCycleLenParam = commonVoiceSate->trainDutyCycleLenParam->load();
  snapShot.trainSilenceParam = commonVoiceSate->trainSilenceParam->load();
  snapShot.trainLenParam = commonVoiceSate->trainLenParam->load();

  snapShot.trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  snapShot.trainTime = snapShot.trainLenParam * snapShot.trainLenBlock;
  snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainSilenceParam + snapShot.trainDutyCycleLenParam);
  snapShot.fundamentalFreq = 1.0 / snapShot.pulsarPeriodTime;
  snapShot.trainSilenceTime = snapShot.trainSilenceParam * snapShot.pulsarPeriodTime;
  snapShot.trainDutyCycleTime = snapShot.trainDutyCycleLenParam * snapShot.pulsarPeriodTime;

  // train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
  snapShot.trainPeriodTime = snapShot.trainDutyCycleTime + snapShot.trainSilenceTime;

  snapShot.dutyCycleRatio = getCurrentDutyCycleRatio(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleRatioDepthParam->load());
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load());
  snapShot.dutyCycleTime = snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime;
  snapShot.pulsarSilenceTime = (1 - snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime;

  float sampleRate = (float)why::sampleRate.load();
  if (sampleRate <= 0.0f)
    sampleRate = 44100.0f;
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime * sampleRate);
  snapShot.pulsarIntraSilenceSamples = std::max(1.0f, snapShot.pulsarPeriodTime * sampleRate - snapShot.pulsarDutyCycleSamples);
  snapShot.interTrainSilenceSamples = std::max(1.0f, snapShot.trainSilenceTime * sampleRate);
  snapShot.trainDutyCycleSamples = std::max(1.0f, snapShot.trainDutyCycleTime * sampleRate);

  trainTotalSamples = static_cast<int>(snapShot.trainDutyCycleSamples);
  if (trainTotalSamples <= 0)
    trainTotalSamples = 1;
}

// 只更新快照中的pulsar数据，因为pulsar period长度已定，但duty cycle raito, duty cycle cluster可以变化，每个pulsar阶段切换都要更新
void PulsarSynthVoice::refreshPulsarInSnapShot() {
  snapShot.dutyCycleRatio = getCurrentDutyCycleRatio(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleRatioDepthParam->load());
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load());
  snapShot.dutyCycleTime = snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime;
  snapShot.pulsarSilenceTime = (1 - snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime;

  float sr = (float)why::sampleRate.load();
  if (sr <= 0.0f)
    sr = 44100.0f;
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime * sr);
  snapShot.pulsarIntraSilenceSamples = std::max(1.0f, snapShot.pulsarPeriodTime * sr - snapShot.pulsarDutyCycleSamples);
}

// auto：自动播放
void PulsarSynthVoice::renderNextBlockDirectly(juce::AudioSampleBuffer &outputBuffer, juce::AudioPlayHead *audioPlayHead, int startSample, int numSamples) {
  // Control the playback and stop of the DAW to trigger the start and stop of the pulsar process respectively
  if (audioPlayHead != nullptr && audioPlayHead->getPosition()) {
    bool isNowPlaying = audioPlayHead->getPosition()->getIsPlaying();

    if (isNowPlaying && !wasPlayingLastFrame) {
      // Transport just started
      isActive = true;
      soundOffWhenSwitchPlayMode = false;

      resetTrain();
      // envelope坐标会一直延续下去，不随train变化而变化，从新播放时才重置
      envIndex = 0;
    }

    if (!isNowPlaying && wasPlayingLastFrame) {
      // Transport just stopped
      isActive = false;
    }

    wasPlayingLastFrame = isNowPlaying;
  }

  if (isActive && !soundOffWhenSwitchPlayMode) {
    processBlockSamples(outputBuffer, startSample, numSamples);
  }
}

void PulsarSynthVoice::processBlockSamples(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) {
  commonVoiceSate->pulseBuffer.clear();
  if (commonVoiceSate->pulseBuffer.getNumSamples() < numSamples || commonVoiceSate->pulseBuffer.getNumChannels() < outputBuffer.getNumChannels()) {
    commonVoiceSate->pulseBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, true, true); // 自动释放并重新分配
  }

  for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++) {
    int i = sampleIndex - startSample;
    auto result = this->processSample();
    commonVoiceSate->pulseBuffer.setSample(0, i, result.first);
    if (commonVoiceSate->pulseBuffer.getNumChannels() > 1)
      commonVoiceSate->pulseBuffer.setSample(1, i, result.first);
  }

  // cache the output gain once per block instead of recomputing std::pow per sample/channel
  const float outputGain = getOutputGain();
  const int numChannels = outputBuffer.getNumChannels();

  for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++) {
    int i = sampleIndex - startSample;
    for (int chan = 0; chan < numChannels; chan++) {
      const auto *pulseReadPtr = commonVoiceSate->pulseBuffer.getReadPointer(chan);
      // fast tanh soft-clip approximation instead of std::tanh
      const float s = juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i]) * outputGain;
      outputBuffer.addSample(chan, sampleIndex, s);
    }
  }
}

void PulsarSynthVoice::resetTrainInitialSate() {
  // 初始状态为pulsar silence阶段，刚进入trian，都要changeStage，会直接进入下一个pulse阶段
  currentState = why::PulsarStateEnum::IntraSilence;
  hasPassedSampleNumInsideTrain = 0;
  currentStateDurationSampleNum = 0;
  endPosInTrainSamples = 0; // the end position of current state
  pulsarStageIndexInTrainDutyCycle = 0;
  pulsaretPhase = 0.0f;
  // envIndex = 0;
  trainSampleCounter = 0;
  firstTrain = true;
  for (auto &g : waveformGrains) {
    g.active = false;
  }
  waveformGrainWriteIdx = 0;
  setEnterNextTrain(true);
}

void PulsarSynthVoice::resetTrain() {
  saveSnapShot();
  refreshPulsaretAdsr(snapShot.dutyCycleTime);
  resetTrainInitialSate();
}

void PulsarSynthVoice::initSynthVoice(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>> &sampleBuffer, juce::AudioPlayHead *audioPlayHead) {
  juce::ignoreUnused(audioPlayHead);
  commonVoiceSate->sampleBuffer = std::move(sampleBuffer);
  if (sampleRate > 0.0)
    why::sampleRate.store(sampleRate);

  // Reset Perlin noise time for fresh PRF modulation
  perlinTime = 0.0;

  // Prepare per-voice LFO modulator with initial frequency
  lfoModulator.prepare(sampleRate, 1.0f);
  saveSnapShot();

  commonVoiceSate->generateStochasticMask();
  // 刷新adsr
  refreshPulsaretAdsr(snapShot.dutyCycleTime);
}

void PulsarSynthVoice::mappingParams(const juce::AudioProcessorValueTreeState &apvts) {
  commonVoiceSate->bpm = apvts.getRawParameterValue(why::ParameterID::bpm);
  // grain size & grain wet
  commonVoiceSate->grainSizeParam = apvts.getRawParameterValue(why::ParameterID::grainSize);
  commonVoiceSate->grainWetParam = apvts.getRawParameterValue(why::ParameterID::grainWet);

  // output
  commonVoiceSate->outputGainParam = apvts.getRawParameterValue(why::ParameterID::outputGain);

  // play mode
  commonVoiceSate->playModeParam = apvts.getRawParameterValue(why::ParameterID::playMode);

  // train
  commonVoiceSate->trainLenParam = apvts.getRawParameterValue(why::ParameterID::trainLen);
  commonVoiceSate->trainDutyCycleLenParam = apvts.getRawParameterValue(why::ParameterID::trainDutyCycleLen);
  commonVoiceSate->trainSilenceParam = apvts.getRawParameterValue(why::ParameterID::trainSilenceLen);

  // pulsar basic info
  commonVoiceSate->pulsarWaveformParam = apvts.getRawParameterValue(why::ParameterID::pulsarWaveform);
  // commonVoiceSate->pulsarDutyCycleClusterLenParam = apvts.getRawParameterValue(why::ParameterID::pulsarDutyCycleClusterLen);
  // commonVoiceSate->pulsarDutyCycleRatioParam = apvts.getRawParameterValue(why::ParameterID::pulsarDutyCycleRatio);

  //  envelope depth
  commonVoiceSate->ampLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::ampLfoDepth);
  commonVoiceSate->formantFreqLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::formantFreqLfoDepth);
  commonVoiceSate->dutyCycleRatioDepthParam = apvts.getRawParameterValue(why::ParameterID::dutyCycleRatioDepth);
  commonVoiceSate->dutyCycleClusterDepthParam = apvts.getRawParameterValue(why::ParameterID::dutyCycleClusterDepth);

  // pulsar envelope
  commonVoiceSate->attackParam = apvts.getRawParameterValue(why::ParameterID::pulsarAttack);
  commonVoiceSate->decayParam = apvts.getRawParameterValue(why::ParameterID::pulsarDecay);
  commonVoiceSate->sustainParam = apvts.getRawParameterValue(why::ParameterID::pulsarSustain);
  commonVoiceSate->releaseParam = apvts.getRawParameterValue(why::ParameterID::pulsarRelease);

  // masking
  commonVoiceSate->maskOptionParam = apvts.getRawParameterValue(why::ParameterID::maskOption);
  commonVoiceSate->maskOption = static_cast<why::MaskOptionEnum>(static_cast<int>(commonVoiceSate->maskOptionParam->load()));

  // burst mask
  // texteditor must be passed through property and is not directly bound by
  // attachment
  if (!apvts.state.getProperty(why::PropertyID::burstMask).isVoid()) {
    // get texteditor value from property because juce can't bind texteditor
    // with parameter automatally
    commonVoiceSate->burstMask = apvts.state.getProperty(why::PropertyID::burstMask).toString().toStdString();
  }

  // stochastic mask
  if (!apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid()) {
    commonVoiceSate->stochasticMaskStr = apvts.state.getProperty(why::PropertyID::stochasticMask).toString().toStdString();
  }

  // euclid mask
  commonVoiceSate->euclidStepsParam = apvts.getRawParameterValue(why::ParameterID::euclidSteps);
  commonVoiceSate->euclidHitsParam = apvts.getRawParameterValue(why::ParameterID::euclidHits);

  if (commonVoiceSate->euclidStepsParam->load() > 0 && commonVoiceSate->euclidHitsParam->load() > 0) {
    // generate euclid rhythm pattern
    commonVoiceSate->euclids = why::generateEuclidRhythm(commonVoiceSate->euclidStepsParam->load(), commonVoiceSate->euclidHitsParam->load());
  }

  // impulse switch
  commonVoiceSate->impulseSwitchParam = apvts.getRawParameterValue(why::ParameterID::impulseSwitch);
}

std::mutex parameterMutex;

void PulsarSynthVoice::mappingOneParam(const juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue) {
  std::lock_guard<std::mutex> lock(parameterMutex);

  if (parameterID == why::ParameterID::grainSize) {
    commonVoiceSate->grainSizeParam = apvts.getRawParameterValue(why::ParameterID::grainSize);
  }
  if (parameterID == why::ParameterID::grainWet) {
    commonVoiceSate->grainWetParam = apvts.getRawParameterValue(why::ParameterID::grainWet);
  }

  // output
  if (parameterID == why::ParameterID::outputGain) {
    commonVoiceSate->outputGainParam->store(newValue);
  }

  // play mode
  if (parameterID == why::ParameterID::playMode) {
    commonVoiceSate->playModeParam->store(newValue);
  }

  // train
  if (parameterID == why::ParameterID::trainLen) {
    commonVoiceSate->trainLenParam->store(newValue);
  }
  if (parameterID == why::ParameterID::trainDutyCycleLen) {
    commonVoiceSate->trainDutyCycleLenParam->store(newValue);
  }
  if (parameterID == why::ParameterID::trainSilenceLen) {
    commonVoiceSate->trainSilenceParam->store(newValue);
  }

  // pulsar basic info
  if (parameterID == why::ParameterID::pulsarWaveform) {
    commonVoiceSate->pulsarWaveformParam->store(newValue);
  }

  // envelope  depth
  if (parameterID == why::ParameterID::ampLfoDepth) {
    commonVoiceSate->ampLfoDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::formantFreqLfoDepth) {
    commonVoiceSate->formantFreqLfoDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::dutyCycleRatioDepth) {
    commonVoiceSate->dutyCycleRatioDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::dutyCycleClusterDepth) {
    commonVoiceSate->dutyCycleClusterDepthParam->store(newValue);
  }

  // pulsar envelope
  if (parameterID == why::ParameterID::pulsarAttack) {
    commonVoiceSate->attackParam->store(newValue);
  }
  if (parameterID == why::ParameterID::pulsarDecay) {
    commonVoiceSate->decayParam->store(newValue);
  }
  if (parameterID == why::ParameterID::pulsarSustain) {
    commonVoiceSate->sustainParam->store(newValue);
  }
  if (parameterID == why::ParameterID::pulsarRelease) {
    commonVoiceSate->releaseParam->store(newValue);
  }

  // masking
  if (parameterID == why::ParameterID::maskOption) {
    commonVoiceSate->maskOptionParam->store(newValue);
    commonVoiceSate->maskOption = static_cast<why::MaskOptionEnum>(static_cast<int>(commonVoiceSate->maskOptionParam->load()));
  }

  // burst mask
  // texteditor must be passed through property and is not directly bound by
  // attachment
  if (!apvts.state.getProperty(why::PropertyID::burstMask).isVoid()) {
    commonVoiceSate->burstMask = apvts.state.getProperty(why::PropertyID::burstMask).toString().toStdString();
  }

  // stochastic mask
  if (!apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid()) {
    commonVoiceSate->stochasticMaskStr = apvts.state.getProperty(why::PropertyID::stochasticMask).toString().toStdString();
  }

  // euclid mask
  if (parameterID == why::ParameterID::euclidSteps) {
    commonVoiceSate->euclidStepsParam->store(newValue);
  }
  if (parameterID == why::ParameterID::euclidHits) {
    commonVoiceSate->euclidHitsParam->store(newValue);
  }

  if (commonVoiceSate->euclidStepsParam->load() > 0 && commonVoiceSate->euclidHitsParam->load() > 0) {
    // generate euclid rhythm pattern
    commonVoiceSate->euclids = why::generateEuclidRhythm(commonVoiceSate->euclidStepsParam->load(), commonVoiceSate->euclidHitsParam->load());
  }

  // impulse switch
  if (parameterID == why::ParameterID::impulseSwitch) {
    commonVoiceSate->impulseSwitchParam->store(newValue);
  }
}

void PulsarSynthVoice::parameterChanged(juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue, bool &isGeneratedStochasticMask) {
  mappingOneParam(apvts, parameterID, newValue);

  // Wait for the next pulsar silence or train interval silence to start a new train，只做标记：可以进入下一个train
  if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen || parameterID == why::ParameterID::trainLen || parameterID == why::ParameterID::bpm ||
      parameterID == why::ParameterID::dutyCycleRatioDepth || parameterID == why::ParameterID::dutyCycleClusterDepth) {
    setEnterNextTrain(true);
    if (parameterID == why::ParameterID::trainDutyCycleLen) {
      // 根据train长度生成对应随机mask
      commonVoiceSate->generateStochasticMask();
      isGeneratedStochasticMask = true;
      if (isGeneratedStochasticMask) {
        if (apvts.state.getProperty(why::PropertyID::stochasticMask) != commonVoiceSate->stochasticMaskStr.data()) {
          apvts.state.setProperty(why::PropertyID::stochasticMask, commonVoiceSate->stochasticMaskStr.data(), nullptr);
        }
      }
    }
  }
}

void PulsarSynthVoice::reloadPreset(juce::AudioProcessorValueTreeState &apvts) {
  mappingParams(apvts);
  // init train
  saveSnapShot();
  refreshPulsaretAdsr(snapShot.dutyCycleTime);
}

// adsr应用在原始的pulsar duty cycle上，经过mask处理后，可以应用在pulsar silence长度上，而非经过duty cycle ratio or cluster处理后的单个pulse上
void PulsarSynthVoice::refreshPulsaretAdsr(float pulsaretTime) {
  // Only push the sample rate to the ADSR when it actually changes (avoid redundant work on the audio thread)
  double sr = getSampleRate();
  if (sr <= 0.0)
    sr = 44100.0;
  pulsarAdsr.setSampleRate(sr);

  float attack = commonVoiceSate->attackParam->load();
  float decay = commonVoiceSate->decayParam->load();
  float release = commonVoiceSate->releaseParam->load();

  // Control ratio(a+d+r) <= 1.0 cut the redundant part
  if (attack + decay + release > 1 && attack + decay >= 1) {
    release = 0;
    if (attack >= 1) {
      decay = 0;
    }
    if (attack < 1) {
      decay = 1 - attack;
    }
  }
  if (attack + decay + release > 1 && attack + decay < 1) {
    release = 1 - attack - decay;
  }

  const float newAttack = pulsaretTime * attack;
  const float newDecay = pulsaretTime * decay;
  const float newSustain = commonVoiceSate->sustainParam->load();
  const float newRelease = pulsaretTime * release;

  pulsarAdsrParams.attack = newAttack;
  pulsarAdsrParams.decay = newDecay;
  pulsarAdsrParams.release = newRelease;
  pulsarAdsrParams.sustain = newSustain;
  pulsarAdsr.setParameters(pulsarAdsrParams);
}

bool PulsarSynthVoice::applyMaskString(const std::string &maskStr, bool &existMask, bool &maskPassFlag) {
  if (!maskStr.empty() && pulsarStageIndexInTrainDutyCycle > 0) {
    existMask = true;
    int index = (pulsarStageIndexInTrainDutyCycle - 1) % static_cast<int>(maskStr.size());
    maskPassFlag = maskStr[index] == '1';
    return true;
  }
  return false;
}

void PulsarSynthVoice::mask(bool &maskPassFlag, bool &existMask) {
  if (commonVoiceSate->maskOption == why::MaskOptionEnum::Off) {
    existMask = false;
    maskPassFlag = true;
    return;
  }
  switch (commonVoiceSate->maskOption) {
  case why::MaskOptionEnum::BurstMask:
    applyMaskString(commonVoiceSate->burstMask, existMask, maskPassFlag);
    break;
  case why::MaskOptionEnum::EuclidMask:
    applyMaskString(commonVoiceSate->euclids, existMask, maskPassFlag);
    break;
  case why::MaskOptionEnum::StochasticMask:
    applyMaskString(commonVoiceSate->stochasticMaskStr, existMask, maskPassFlag);
    break;
  default:
    break;
  }
}

void PulsarSynthVoice::spawnGrain() {
  uint8_t next = 0; // 一维数组celluar
  for (int b = 0; b < 8; ++b) {
    int left = (caState >> ((b + 7) & 7)) & 1;
    int current = (caState >> b) & 1;
    int right = (caState >> ((b + 1) & 7)) & 1;
    next |= ((left ^ (current | right)) << b);
  }
  caState = next;
  int active = 0;
  for (int j = 0; j < 8; ++j)
    active += (caState >> j) & 1;
  float caPitchOffset = 0.9f + 0.3f * (static_cast<float>(active) / 8.0f);

  float envPhase = 1.0f * envIndex / 2048;
  float triggerFreq = snapShot.fundamentalFreq * calcFormantLfoInterpolation(envPhase, commonVoiceSate->formantFreqLfoDepthParam->load()) //
                      * getCurrentDutyCycleCluster(envPhase, commonVoiceSate->dutyCycleClusterDepthParam->load())                         //
                      * caPitchOffset                                                                                                     //
                      / getCurrentDutyCycleRatio(envPhase, commonVoiceSate->dutyCycleRatioDepthParam->load())                             //
      ;
  float phaseInc = (triggerFreq > 0.0f) ? triggerFreq / static_cast<float>(getSampleRate()) : 1.0f / static_cast<float>(getSampleRate());
  waveformGrains[waveformGrainWriteIdx] = {0, phaseInc, true, currentStateDurationSampleNum, //
                                           calcAmpLfoInterpolation(envPhase, commonVoiceSate->ampLfoDepthParam->load())};
  waveformGrainWriteIdx = (waveformGrainWriteIdx + 1) % MAX_WAVEFORM_GRAINS;

  envIndex++;
  if (envIndex >= 2048)
    envIndex = 2047;
}

/**
 * pulsarSilenceSamples: pulsar silence samples
 * interTrainSilenceSamples: silence samples between two trains
 */
void PulsarSynthVoice::changeStage() {
  hasPassedSampleNumInsideTrain = 0;
  // The silence of the mask may also become a pulse. A pulsar cluster can contain multiple pulsaret, and this is the final smallest phase
  pulsaretPhase = 0.0f;

  switch (currentState) {
  case why::PulsarStateEnum::Pulse: // The duty cycle stage of pulsar has been completed at present
    currentState = why::PulsarStateEnum::IntraSilence;
    currentStateDurationSampleNum = snapShot.pulsarIntraSilenceSamples;
    // The total duration corresponding to the new state
    endPosInTrainSamples += snapShot.pulsarIntraSilenceSamples;
    pulsarStageIndexInTrainDutyCycle++;
    break;
  case why::PulsarStateEnum::IntraSilence: // The silence stage of pulsar has now been completed
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = snapShot.pulsarDutyCycleSamples;
    endPosInTrainSamples += snapShot.pulsarDutyCycleSamples;
    pulsarStageIndexInTrainDutyCycle++;
    // spawn a waveform grain: capture fmModulation at trigger time as fixed phase increment
    spawnGrain();
    break;
  case why::PulsarStateEnum::InterTrainSilence: // train silence finished
    firstTrain = false;
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = snapShot.pulsarDutyCycleSamples;
    endPosInTrainSamples = snapShot.pulsarDutyCycleSamples;
    pulsarStageIndexInTrainDutyCycle = 0; // reset mask only after full train finishes
    envIndex = 0;                         // reset envelope scan after full train finishes
    trainSampleCounter = 0;               // new train starts, reset envelope scan
    spawnGrain();
    break;
  }
}

// modulation will affect the length of the pulse
void PulsarSynthVoice::calcNewPulsarFreq(float pulsarDutyCycleRatio, float &pulsarModFreq, bool silenceToPulseFlag, int cluster) {
  // 开始的单个pulse频率
  pulsarModFreq = pulsarDutyCycleRatio <= 0 ? 0 : 1.0 / (pulsarDutyCycleRatio * snapShot.pulsarPeriodTime);
  // 当ratio为1时，实际上也不存在silence了，所以无需计算新的freq
  if (silenceToPulseFlag && pulsarDutyCycleRatio < 1.0f) {
    pulsarModFreq = 1.0 / ((1 - pulsarDutyCycleRatio) * snapShot.pulsarPeriodTime);
  }

  // The frequency of the pulse is finally multiplied by duty cycle
  pulsarModFreq = cluster * pulsarModFreq;
}

float PulsarSynthVoice::getPulseFadeGain() const {
  if (currentStateDurationSampleNum <= 0)
    return 1.0f;

  // Hann 窗口：0.5 * (1 - cos(2*pi * pos / (N-1)))
  int pos = hasPassedSampleNumInsideTrain;
  int N = currentStateDurationSampleNum;
  if (N == 1)
    return 1.0f;

  float hann = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(pos) / static_cast<float>(N - 1)));
  return hann;
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask) {
  switch (currentState) {
  case why::PulsarStateEnum::Pulse:
  case why::PulsarStateEnum::IntraSilence:
    // If a mask exists, sounds may be emitted during the silence stage; otherwise, all are zeros
    if (!existMask && currentState == why::PulsarStateEnum::IntraSilence) {
      return 0;
    }
    // mask is 0
    if (!passMaskFlag) {
      return 0;
    }
    return calcActualPulse() * getPulseFadeGain();
  case why::PulsarStateEnum::InterTrainSilence:
    return 0.0f;
  }
  return 0.0f;
}

// return {sample value, newPulsarFreq}
std::pair<float, float> PulsarSynthVoice::processSample() {
  // once playback, default is loop and can not be updated
  if (!commonVoiceSate->isLoop && !firstTrain) {
    return {0.0, 0.0};
  }

  // train长度为0不处理
  if (snapShot.trainDutyCycleTime <= 0) {
    return {0.0, 0.0};
  }

  // Advance Perlin noise time for smooth PRF modulation
  perlinTime += 100 * (1.0 / getSampleRate());

  // Map envelope index to train progress (0..2048 across train duty cycle)
  // if (trainTotalSamples > 0) {
  //   int wrappedCounter = trainSampleCounter % trainTotalSamples;
  //   envIndex = static_cast<int>((wrappedCounter / static_cast<float>(trainTotalSamples)) * 2048.0f);
  //   if (envIndex >= 2048)
  //     envIndex = 2047;
  // }

  // Enter next stage
  if (hasPassedSampleNumInsideTrain >= currentStateDurationSampleNum) {
    if (enterNextTrain) {
      // Use the latest train configuration from the interface as the next train configuration，正式进入下一个train
      resetTrain();
      enterNextTrain = false;
    }
    // 刷新duty cycle ratio, duty cycle cluseter
    refreshPulsarInSnapShot();
    changeStage();

    if (currentStateDurationSampleNum <= 0) { // if the next state is consist of 0 sample
      currentStateDurationSampleNum = 1;
    }

    // reset to an idle
    pulsarAdsr.reset();
    refreshPulsaretAdsr(snapShot.dutyCycleTime);

    pulsarAdsr.noteOn();
    isTriggeredReleaseFlag = false;
  }

  bool passMaskFlag = true; // the mask of current sample is 1
  bool existMask = false;

  // train silence不走mask
  if (currentState != why::PulsarStateEnum::InterTrainSilence) {
    mask(passMaskFlag, existMask);
  }

  // It used to be pulsar silence and now it has become pulse. To confirm the final frequency of pulsar and the adsr ratio value
  bool silenceToPulseFlag = currentState == why::PulsarStateEnum::IntraSilence && existMask && passMaskFlag;

  // The frequency after pulse duty cycle modulation will affect the length of the pulse
  float newPulsarFreq;
  calcNewPulsarFreq(snapShot.dutyCycleRatio, newPulsarFreq, silenceToPulseFlag, static_cast<int>(snapShot.dutyCycleCluster));

  // Trigger the release phase: Within the application duration, there should still be room to apply the release; otherwise, it will not be triggered
  if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && currentStateDurationSampleNum - hasPassedSampleNumInsideTrain <= pulsarAdsrParams.release * getSampleRate()) {
    pulsarAdsr.noteOff();
    isTriggeredReleaseFlag = true;
  }

  // calc sample
  float s = calSampleByState(passMaskFlag, existMask);

  hasPassedSampleNumInsideTrain++;
  trainSampleCounter++;
  return {s, newPulsarFreq};
}

inline float formantFreqToDrive(float f) {
  // f: 0..1
  return 1.0f + f * 8.0f;
}

inline float gaussianEnvelope(float t, float sigma = 0.1f) {
  // t: 0.0 - 1.0
  // sigma: 控制宽度 (0.05 ~ 0.4 常用)
  const float x = t - 0.5f;
  const float denom = 2.0f * sigma * sigma;
  return std::exp(-(x * x) / denom);
}

float PulsarSynthVoice::calcActualPulse() {
  // formant frequency, emmission frequency完全无关
  float waveformSample;
  float grainSum = 0.0f;
  int activeCount = 0;
  float s = 0;

  for (auto &g : waveformGrains) {
    if (!g.active)
      continue;
    float raw = commonVoiceSate->usePgWaveformEnvelope.load() ? getWaveformEnvelopeValueAtPhase(g.phase) : 0.0f;
    raw = juce::jlimit(-1.0f, 1.0f, raw);
    const float hann = gaussianEnvelope(g.phase);
    grainSum += raw * hann * g.amplitude;
    activeCount++;
    g.phase += g.phaseInc;
    if (g.phase >= 1.0f)
      g.phase -= 1;
    // 如果播够一个pulsar duty cycle
    if (--g.remainSamples <= 0) {
      g.active = false;
    }
  }
  waveformSample = activeCount > 0 ? grainSum / static_cast<float>(activeCount) : 0.0f;
  // float envPhase = 1.0f * envIndex / 2048;
  float hann2 = gaussianEnvelope(pulsaretPhase);
  s = waveformSample * pulsarAdsr.getNextSample() * hann2;
  // s = waveformSample * calcAmpLfoInterpolation(pulsaretPhase, commonVoiceSate->ampLfoDepthParam->load()); // AM - 使用 envIndex 从包络采样
  // // *pulsarAdsr.getNextSample() * hann;

  // // // 针对duty cycle做AM，duty cycle内播放的是overlap-add grains，此处也只能针对大范围处理
  pulsaretPhase += static_cast<float>(snapShot.fundamentalFreq) / getSampleRate();
  if (pulsaretPhase >= 1.0f) {
    pulsaretPhase = pulsaretPhase - 1.0f;
  }
  return s;
}

float PulsarSynthVoice::getCurrentDutyCycleCluster(float phase, float depth) {
  if (depth <= 0.0f) {
    return 1.0f;
  }

  if (commonVoiceSate->useDutyCycleClusterEnvelope.load()) {
    float envelopeValue = getDutyCycleClusterEnvelopeValueAtPhase(phase);
    // envelopeValue 范围是 yMin - yMax，需要归一化到合适的调制范围
    float yMin = commonVoiceSate->dutyCycleClusterEnvelopeYMin.load();
    float yMax = commonVoiceSate->dutyCycleClusterEnvelopeYMax.load();
    float modFactor = juce::jmap(envelopeValue, yMin, yMax, 1.0f, 16.0f);
    // 将 envelopeValue 转换为调制系数
    return static_cast<float>(std::round(depth * modFactor));
  }

  return 1.0f;
}

float PulsarSynthVoice::getCurrentDutyCycleRatio(float phase, float depth) {
  if (depth <= 0.0f) {
    return 0.01f;
  }

  if (commonVoiceSate->useDutyCycleRatioEnvelope.load()) {
    float envelopeValue = getDutyCycleRatioEnvelopeValueAtPhase(phase);
    float yMin = commonVoiceSate->dutyCycleRatioEnvelopeYMin.load();
    float yMax = commonVoiceSate->dutyCycleRatioEnvelopeYMax.load();
    float modFactor = juce::jmap(envelopeValue, yMin, yMax, 0.01f, 1.0f);
    return depth * modFactor;
  }

  return 0.01f;
}

float PulsarSynthVoice::calcFormantLfoInterpolation(float phase, float depth) {
  if (depth <= 0.0f) {
    return 0.0f;
  }

  // 如果启用了 FM 包络，从包络数据采样
  if (commonVoiceSate->useFmEnvelope.load()) {
    float envelopeValue = getFmEnvelopeValueAtPhase(phase);
    float yMin = commonVoiceSate->fmEnvelopeYMin.load();
    float yMax = commonVoiceSate->fmEnvelopeYMax.load();
    // envelopeValue 范围是 yMin - yMax (semitones)，转换为频率比例
    float semitones = juce::jlimit(yMin, yMax, envelopeValue);
    // 每八度12个semitone，频率比 = 2^(semitones/12)
    float freqRatio = std::pow(2.0f, semitones / 12.0f);
    // ratio∈[0.25,4.0]
    return freqRatio * depth;
  }
  return 0;
}

float PulsarSynthVoice::calcAmpLfoInterpolation(float phase, float depth) {
  // 如果 depth 为 0，不应用 AM
  if (depth <= 0.0f) {
    return 1;
  }

  // 如果启用了包络，从包络数据采样
  if (commonVoiceSate->useAmpEnvelope.load()) {
    // phase: 0.0 - 1.0，映射到 2048 个点
    float envelopeValue = getAmpEnvelopeValueAtPhase(phase);
    // // envelopeValue 范围是 yMin - yMax，需要归一化到合适的调制范围
    float yMin = commonVoiceSate->ampEnvelopeYMin.load();
    float yMax = commonVoiceSate->ampEnvelopeYMax.load();
    // 将 envelopeValue 转换为调制系数 (0.0 - 1.0 范围)
    float modFactor = juce::jmap(envelopeValue, yMin, yMax, 0.0f, 1.0f);
    return (1.0f - depth + depth * modFactor);
  }
  return 1;
}

float PulsarSynthVoice::getEnvelopeValueAtPhase(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data, float phase, float scale) const {
  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  phase = std::fmod(std::abs(phase), 1.0f);
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;
  float val1 = data[std::min(index, size - 1)] * scale;
  float val2 = data[std::min(index + 1, size - 1)] * scale;
  return val1 + frac * (val2 - val1);
}

float PulsarSynthVoice::getAmpEnvelopeValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->ampEnvelopeData, phase, commonVoiceSate->ampEnvelopeScale.load()); }

float PulsarSynthVoice::getFmEnvelopeValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->fmEnvelopeData, phase, commonVoiceSate->fmEnvelopeScale.load()); }

float PulsarSynthVoice::getDutyCycleRatioEnvelopeValueAtPhase(float phase) const {
  return getEnvelopeValueAtPhase(commonVoiceSate->dutyCycleRatioEnvelopeData, phase, commonVoiceSate->dutyCycleRatioEnvelopeScale.load());
}

float PulsarSynthVoice::getDutyCycleClusterEnvelopeValueAtPhase(float phase) const {
  return getEnvelopeValueAtPhase(commonVoiceSate->dutyCycleClusterEnvelopeData, phase, commonVoiceSate->dutyCycleClusterEnvelopeScale.load());
}

float PulsarSynthVoice::getWaveformEnvelopeValueAtPhase(float phase) const {
  return getEnvelopeValueAtPhase(commonVoiceSate->pgWaveformEnvelopeData, phase, commonVoiceSate->pgWaveformEnvelopeScale.load());
}

// db to gain
float PulsarSynthVoice::getOutputGain() { return commonVoiceSate->outputGainParam == nullptr ? 1 : std::pow(10.0f, commonVoiceSate->outputGainParam->load() / 20.0f); }

// 当 PRF 较高（例如 > 50Hz）：粒子紧密相连，人耳无法分辨单个粒子，它们融合在一起，形成一个连续的音调或厚重的 Pad 音色。
// 此时 PRF 调制会让声音产生类似“合唱”或“相位移动”的流动感。
// 当 PRF 较低（例如 < 20Hz）：人耳能清晰地听到“哒、哒、哒”的单个脉冲。
// 此时 PRF 调制就变成了节奏调制（Rhythmic Modulation），你可以用它来做极其复杂的、非线性的 Glitch 节奏或序列。
float PulsarSynthVoice::getPerlinPrfModulation() const {
  float noiseVal = perlinNoise.noise(static_cast<float>(perlinTime * 2.0));
  return 1.0f + noiseVal * 0.05f; // 5% smooth PRF variation
}
