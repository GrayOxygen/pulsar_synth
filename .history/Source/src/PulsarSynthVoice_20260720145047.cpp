//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/PulsarSynthVoice.h"
#include "../include/CommonVoiceSate.h"
#include "../include/PulsarSynthSound.h"
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

  snapShot.pulsarFreq = 1.0 / snapShot.dutyCycleTime;
  float sampleRate = getSampleRate();
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime * sampleRate);
  snapShot.pulsarIntraSilenceSamples = std::max(1.0f, snapShot.pulsarPeriodTime * sampleRate - snapShot.pulsarDutyCycleSamples);
  snapShot.interTrainSilenceSamples = std::max(1.0f, snapShot.trainSilenceTime * sampleRate);
  snapShot.trainDutyCycleSamples = std::max(1.0f, snapShot.trainDutyCycleTime * sampleRate);
}

// 只更新快照中的pulsar数据，因为pulsar period长度已定，但duty cycle raito, duty cycle cluster可以变化，每个pulsar阶段切换都要更新
void PulsarSynthVoice::refreshPulsarInSnapShot() {
  snapShot.dutyCycleRatio = getCurrentDutyCycleRatio(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleRatioDepthParam->load());
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load());
  snapShot.dutyCycleTime = snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime;
  snapShot.pulsarSilenceTime = (1 - snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime;
  snapShot.pulsarFreq = 1.0 / snapShot.dutyCycleTime;

  float sr = getSampleRate();
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

      // envelope坐标会一直延续下去，不随train变化而变化，从新播放时才重置
      envIndex = 0;
      // mask
      pulsarStageIndexInTrainDutyCycle = 0;
      // 不清空活跃的waveform grains，让它们自然播放完毕，避免声音突然中断
      for (auto &g : waveformGrains) {
        g.active = false;
        g.phase = 0.0; // 重置相位，保证新train的第一个grain从波形起点开始扫描
      }
      waveformGrainWriteIdx = 0;
      resetTrain();
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
      // const float s = dcBlocker.process(juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i])) * outputGain;
      const float s = juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i]) * outputGain;
      // const float s = dcBlocker.process(pulseReadPtr[i]) * outputGain;
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
  // pulsarStageIndexInTrainDutyCycle = 0;
  // envIndex = 0;
  firstTrain = true;
  // // 不清空活跃的waveform grains，让它们自然播放完毕，避免声音突然中断
  // for (auto &g : waveformGrains) {
  //   g.active = false;
  //   g.phase = 0.0; // 重置相位，保证新train的第一个grain从波形起点开始扫描
  // }
  // waveformGrainWriteIdx = 0;
  setEnterNextTrain(true);
}

void PulsarSynthVoice::resetTrain() {
  saveSnapShot();
  refreshPulsaretAdsr(snapShot.dutyCycleTime);
  resetTrainInitialSate();
}

void PulsarSynthVoice::initSynthVoice(double sampleRate, juce::AudioPlayHead *audioPlayHead) {
  juce::ignoreUnused(audioPlayHead);
  if (sampleRate > 0.0)
    why::sampleRate.store(sampleRate);

  // Reset Perlin noise time for fresh PRF modulation
  perlinTime = 0.0;

  saveSnapShot();

  commonVoiceSate->generateStochasticMask();
  // 刷新adsr
  refreshPulsaretAdsr(snapShot.dutyCycleTime);
}

void PulsarSynthVoice::mappingParams(const juce::AudioProcessorValueTreeState &apvts) {
  commonVoiceSate->bpm = apvts.getRawParameterValue(why::ParameterID::bpm);

  // output
  commonVoiceSate->outputGainParam = apvts.getRawParameterValue(why::ParameterID::outputGain);

  // train
  commonVoiceSate->trainLenParam = apvts.getRawParameterValue(why::ParameterID::trainLen);
  commonVoiceSate->trainDutyCycleLenParam = apvts.getRawParameterValue(why::ParameterID::trainDutyCycleLen);
  commonVoiceSate->trainSilenceParam = apvts.getRawParameterValue(why::ParameterID::trainSilenceLen);

  // pulsar basic info
  commonVoiceSate->pulsarWaveformParam = apvts.getRawParameterValue(why::ParameterID::pulsarWaveform);

  //  envelope depth
  commonVoiceSate->amEnvDepthParam = apvts.getRawParameterValue(why::ParameterID::amEnvelopeDepth);
  commonVoiceSate->fmEnvelopeDepthParam = apvts.getRawParameterValue(why::ParameterID::fmEnvelopeDepth);
  commonVoiceSate->dutyCycleRatioDepthParam = apvts.getRawParameterValue(why::ParameterID::dutyCycleRatioDepth);
  commonVoiceSate->dutyCycleClusterDepthParam = apvts.getRawParameterValue(why::ParameterID::dutyCycleClusterDepth);
  commonVoiceSate->fmLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::fmLfoDepth);
  commonVoiceSate->amLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::amLfoDepth);

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
}

std::mutex parameterMutex;

void PulsarSynthVoice::mappingOneParam(const juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue) {
  std::lock_guard<std::mutex> lock(parameterMutex);

  // output
  if (parameterID == why::ParameterID::outputGain) {
    commonVoiceSate->outputGainParam->store(newValue);
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
  if (parameterID == why::ParameterID::amEnvelopeDepth) {
    commonVoiceSate->amEnvDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::fmEnvelopeDepth) {
    commonVoiceSate->fmEnvelopeDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::dutyCycleRatioDepth) {
    commonVoiceSate->dutyCycleRatioDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::dutyCycleClusterDepth) {
    commonVoiceSate->dutyCycleClusterDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::fmLfoDepth) {
    commonVoiceSate->fmLfoDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::amLfoDepth) {
    commonVoiceSate->amLfoDepthParam->store(newValue);
  }
  // pulsar envelope∏
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
}

void PulsarSynthVoice::parameterChanged(juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue, bool &isGeneratedStochasticMask) {
  mappingOneParam(apvts, parameterID, newValue);

  // Wait for the next pulsar silence or train interval silence to start a new train，只做标记：可以进入下一个train
  if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen || parameterID == why::ParameterID::trainLen || parameterID == why::ParameterID::bpm) {
    // if train params were changed
    if (snapShot.bpm != commonVoiceSate->bpm->load() || snapShot.trainSilenceParam != commonVoiceSate->trainSilenceParam->load() || snapShot.trainLenParam != commonVoiceSate->trainLenParam->load() ||
        snapShot.trainDutyCycleLenParam != commonVoiceSate->trainDutyCycleLenParam->load()) {
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
  if (parameterID == why::ParameterID::dutyCycleRatioDepth || parameterID == why::ParameterID::dutyCycleClusterDepth) {
    if (snapShot.dutyCycleRatio != getCurrentDutyCycleRatio(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleRatioDepthParam->load()) ||
        snapShot.dutyCycleCluster != getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load())) {
      refreshPulsarInSnapShot();
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

/**
 * pulsarSilenceSamples: pulsar silence samples
 * interTrainSilenceSamples: silence samples between two trains
 */
void PulsarSynthVoice::changeStage() {
  hasPassedSampleNumInsideTrain = 0;

  switch (currentState) {
  case why::PulsarStateEnum::Pulse: // The duty cycle stage of pulsar has been completed at present
    currentState = why::PulsarStateEnum::IntraSilence;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarIntraSilenceSamples);
    // The total duration corresponding to the new state
    endPosInTrainSamples += static_cast<int>(snapShot.pulsarIntraSilenceSamples);
    pulsarStageIndexInTrainDutyCycle++;
    break;
  case why::PulsarStateEnum::IntraSilence: // The silence stage of pulsar has now been completed
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    endPosInTrainSamples += static_cast<int>(snapShot.pulsarDutyCycleSamples);
    pulsarStageIndexInTrainDutyCycle++;
    // spawn a waveform grain: capture fmModulation at trigger time as fixed phase increment
    spawnGrain();
    break;
  case why::PulsarStateEnum::InterTrainSilence: // train silence finished  TODO 没用到，直接删除了
    firstTrain = false;
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    endPosInTrainSamples = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    // pulsarStageIndexInTrainDutyCycle = 0;
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
inline float hannWindow(float phase) {
  phase = juce::jlimit(0.0f, 1.0f, phase);
  return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));
}

inline float gaussianEnvelope(float t, float sigma = 0.3f) {
  const float x = t - 0.5f;
  const float denom = 2.0f * sigma * sigma;
  float y = std::exp(-(x * x) / denom);
  // 边界值
  const float edge = std::exp(-(0.5f * 0.5f) / denom);
  // 映射到 [0,1]
  y = (y - edge) / (1.0f - edge);
  return juce::jlimit(0.0f, 1.0f, y);
}

float PulsarSynthVoice::getPulseFadeGain() const {
  if (currentStateDurationSampleNum <= 0)
    return 1.0f;

  int pos = hasPassedSampleNumInsideTrain;
  int N = currentStateDurationSampleNum;
  if (N == 1)
    return 1.0f;
  // return gaussianEnvelope(static_cast<float>(pos) / static_cast<float>(N - 1), 0.5);
  return hannWindow(static_cast<float>(pos) / static_cast<float>(N - 1));
}

// return {sample value, newPulsarFreq}
std::pair<float, float> PulsarSynthVoice::processSample() {
  // Advance Perlin noise time for smooth modulation
  perlinTime += (1.0 / getSampleRate());

  // Enter next stage
  if (hasPassedSampleNumInsideTrain >= currentStateDurationSampleNum) {
    bool wasEnterNextTrain = enterNextTrain.load();
    if (wasEnterNextTrain) {
      // Use the latest train configuration from the interface as the next train configuration，正式进入下一个train
      resetTrain();
      enterNextTrain = false;
    }
    // 刷新duty cycle ratio, duty cycle cluseter
    refreshPulsarInSnapShot();
    changeStage();

    // while (currentStateDurationSampleNum <= 0) {
    //   changeStage();
    // }

    pulsarAdsr.noteOff();
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
  if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && (currentStateDurationSampleNum - hasPassedSampleNumInsideTrain) <= pulsarAdsrParams.release * getSampleRate()) {
    pulsarAdsr.noteOff();
    isTriggeredReleaseFlag = true;
  }

  // calc sample
  float s = calSampleByState(passMaskFlag, existMask);

  hasPassedSampleNumInsideTrain++;
  return {s, newPulsarFreq};
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask) {
  switch (currentState) {
  case why::PulsarStateEnum::Pulse:
  case why::PulsarStateEnum::IntraSilence: {
    // If a mask exists, sounds may be emitted during the silence stage; otherwise, all are zeros
    if (!existMask && currentState == why::PulsarStateEnum::IntraSilence) {
      return 0;
    }
    // mask is 0
    if (!passMaskFlag) {
      return 0;
    }
    float gain = getPulseFadeGain();
    return calcActualPulse() * gain;
  }
  case why::PulsarStateEnum::InterTrainSilence:
    return 0.0f;
  }
}

int PulsarSynthVoice::getCellActiveCount() {
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

  return active;
}

void PulsarSynthVoice::spawnGrain() {
  int active = getCellActiveCount();
  double perlinMod = getPerlinModulation();
  double caPitchOffset = perlinMod; //  perlin
  double startPhaseOffset = 0.5f * static_cast<float>(active) / 8.0f;
  float scale = 0.9f + 0.2f * static_cast<float>(active) / 8.0f;
  // caPitchOffset = 1.0f;
  double envPhase = 1.0f * envIndex / 2048;
  // 获取实时envelope; 每个sample，对应一个envelope点，确定fm，am，dc ratio，dc cluster，然后用确定的freq扫描waveform
  // 基准频率与pulsar period绑定：一个pulsar period恰好扫完一个完整波形周期，与bpm/train长度无关
  // double baseFreq = snapShot.pulsarPeriodTime > 0.0f ? 1.0 / snapShot.pulsarPeriodTime : 0.0;
  double baseFreq = snapShot.fundamentalFreq;
  // 高PRF时衰减per-grain随机音高抖动：脉冲密集(>20Hz)融合成音高后，周期抖动会把线状谱抹成噪声谱(noise感)；
  // 低PRF时保留全部抖动作为有机感
  double prf = baseFreq;
  double jitterAtten = prf > 20.0 ? 20.0 / prf : 1.0;
  scale = 1.0f + static_cast<float>((scale - 1.0f) * jitterAtten);
  caPitchOffset = 1.0 + (caPitchOffset - 1.0) * jitterAtten;
  double triggerFreq = static_cast<double>(baseFreq) * calcFmEnvelopeInterpolation(envPhase, commonVoiceSate->fmEnvelopeDepthParam->load()) * caPitchOffset;
  double phaseInc = (triggerFreq > 0.0f) ? triggerFreq / static_cast<float>(getSampleRate()) : 1.0f / static_cast<float>(getSampleRate());

  int samples = static_cast<int>(currentStateDurationSampleNum) * static_cast<int>(snapShot.dutyCycleCluster);
  float amp = calcAmEnvelopeInterpolation(envPhase, commonVoiceSate->amEnvDepthParam->load());

  // 相位延续：新grain从上一个grain停下的相位继续扫描波形，保证跨pulse的波形连续（如sine前半周期 -> 后半周期）
  int prevGrainIdx = (waveformGrainWriteIdx + MAX_WAVEFORM_GRAINS - 1) % MAX_WAVEFORM_GRAINS;
  startPhaseOffset = static_cast<double>(waveformGrains[prevGrainIdx].phase);

  waveformGrains[waveformGrainWriteIdx] = {0, phaseInc, true, samples, samples, 1, 0, amp};
  waveformGrainWriteIdx = (waveformGrainWriteIdx + 1) % MAX_WAVEFORM_GRAINS;

  envIndex++;
  if (envIndex >= 2048)
    envIndex = envIndex - 2048;

  for (auto &g : waveformGrains) {
    if (!g.active)
      continue;
    g.samplesInCurrentStage = g.remainSamples;
    if (g.remainSamples > currentStateDurationSampleNum) {
      g.samplesInCurrentStage = currentStateDurationSampleNum;
    }
  }
}

float PulsarSynthVoice::calcActualPulse() {
  // formant frequency, emmission frequency完全无关
  double waveformSample;
  double grainSum = 0.0f;
  int activeCount = 0;
  double s = 0;

  for (auto &g : waveformGrains) {
    if (!g.active)
      continue;

    double raw = getWaveformEnvelopeValueAtPhase(g.phase);
    raw = juce::jlimit(-1.0, 1.0, raw);

    // LFO用grain生命周期相位(windowPhase, 单调0->1)查表：g.phase现在会wrap(1->0)，用它查表会在wrap瞬间跳变产生咔哒声
    double fmLfo = calcFmLfoInterpolation(g.windowPhase, commonVoiceSate->fmLfoDepthParam->load());
    double amLfo = calcAmLfoInterpolation(g.windowPhase, commonVoiceSate->amLfoDepthParam->load());

    // 跨pulsar duty cycle window：per-grain Hann窗，50%重叠时窗和恒定(COLA)，首尾为0，grain生灭无click
    double completeWindow = hannWindow(static_cast<float>(g.windowPhase));
    g.windowPhase += 1.0 / static_cast<double>(g.totalSamples);
    if (g.windowPhase > 1.0)
      g.windowPhase = 1.0;

    // 不再乘全局stage窗(windowInCurrentStage)：它会在每个pulse边界把所有grain一起掐0，破坏overlap平滑；stage边界的防click由getPulseFadeGain负责
    grainSum += raw * g.amp * amLfo; // am modulation
    activeCount++;
    g.phase += g.phaseInc * fmLfo * g.scale; // fm modulation
    if (g.phase >= 1.0f)
      g.phase -= 1;

    // 如果播够一个pulsar duty cycle
    g.remainSamples--;
    if (g.remainSamples <= 0) {
      g.active = false;
    }
  }

  // waveformSample = activeCount > 0 ? grainSum / std::pow(static_cast<float>(activeCount), 0.7f) : 0.0f;
  // waveformSample = juce::dsp::FastMathApproximations::tanh(waveformSample);

  // 平滑归一化：activeCount整数跳变会造成增益突跳(咔哒/毛糙)，用一阶平滑过渡
  smoothedActiveCount += 0.0005f * (static_cast<float>(std::max(1, activeCount)) - smoothedActiveCount);
  waveformSample = activeCount > 0 ? grainSum / std::sqrt(smoothedActiveCount) : 0.0f;
  s = waveformSample * pulsarAdsr.getNextSample();
  return s;
}

int PulsarSynthVoice::getCurrentDutyCycleCluster(float phase, float depth) {
  if (depth <= 0.0f) {
    return 1.0f;
  }

  if (commonVoiceSate->useDutyCycleClusterEnvelope.load()) {
    float envelopeValue = getDutyCycleClusterEnvelopeValueAtPhase(phase);
    return static_cast<int>(depth * envelopeValue);
  }

  return 1.0f;
}

float PulsarSynthVoice::getCurrentDutyCycleRatio(float phase, float depth) {
  if (depth <= 0.0f) {
    return 0.01f;
  }

  if (commonVoiceSate->useDutyCycleRatioEnvelope.load()) {
    float envelopeValue = getDutyCycleRatioEnvelopeValueAtPhase(phase);
    // float yMin = commonVoiceSate->dutyCycleRatioEnvelopeYMin.load();
    // float yMax = commonVoiceSate->dutyCycleRatioEnvelopeYMax.load();
    // float modFactor = juce::jmap(envelopeValue, yMin, yMax, 0.01f, 1.0f);
    return depth * envelopeValue;
  }

  return 0.01f;
}

float PulsarSynthVoice::calcFmEnvelopeInterpolation(float phase, float depth) {
  return getFmEnvelopeValueAtPhase(phase) * depth; //
}

float PulsarSynthVoice::calcAmEnvelopeInterpolation(float phase, float depth) {
  // 如果 depth 为 0，不应用 AM
  if (depth <= 0.0f) {
    return 0;
  }
  if (commonVoiceSate->useAmpEnvelope.load()) {
    float envelopeValue = getAmpEnvelopeValueAtPhase(phase);
    return depth * envelopeValue;
  }
  return 0;
}

float PulsarSynthVoice::calcFmLfoInterpolation(float phase, float depth) {
  float env = getFmLfoValueAtPhase(phase);
  // float semitone = juce::jmap(env, 0.01f, 1.0f, -1.0f, 1.0f);
  float ratio = std::pow(2.0f, semitone / 12.0f);
  return ratio * depth;
  // return (0.5f + (2.0f * getFmLfoValueAtPhase(phase) * depth - 1.0f) / 2.0f) * getPerlinModulation(); // [0, 1]
}

float PulsarSynthVoice::calcAmLfoInterpolation(float phase, float depth) {
  return (0.5f + (2.0f * getAmLfoValueAtPhase(phase) * depth - 1.0f) / 2.0f); // [0, 1]
}

float PulsarSynthVoice::getEnvelopeValueAtPhase(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data, float phase, float scale) const {
  // constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  // phase = std::fmod(std::abs(phase), 1.0f);
  // float indexF = phase * (size - 1);
  // int index = static_cast<int>(indexF);
  // float frac = indexF - index;
  // float val1 = data[std::min(index, size - 1)] * scale;
  // float val2 = data[std::min(index + 1, size - 1)] * scale;
  // return val1 + frac * (val2 - val1);

  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  phase -= std::floor(phase); // 比 fmod 更适合 phase
  float indexF = phase * size;
  int i0 = (int)indexF;
  float frac = indexF - i0;
  i0 %= size;
  int i1 = (i0 + 1) % size;
  float a = data[static_cast<size_t>(i0)] * scale;
  float b = data[static_cast<size_t>(i1)] * scale;
  return a + (b - a) * frac;

  // phase = std::fmod(std::abs(phase), 1.0f);

  // constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  // constexpr float crossfadeRatio = 0.3f; // 最后3%进行crossfade

  // auto sampleAt = [&](float p) {
  //   p = std::fmod(std::abs(p), 1.0f);

  //   float indexF = p * (size - 1);
  //   int index = static_cast<int>(indexF);
  //   float frac = indexF - index;

  //   float v1 = data[index];
  //   float v2 = data[(index + 1) % size];

  //   return v1 + frac * (v2 - v1);
  // };

  // // 普通区域
  // if (phase < 1.0f - crossfadeRatio)
  //   return sampleAt(phase);

  // // 最后crossfade区域
  // float t = (phase - (1.0f - crossfadeRatio)) / crossfadeRatio;

  // float tail = sampleAt(phase);
  // // 映射到开头
  // float headPhase = t * crossfadeRatio;
  // float head = sampleAt(headPhase);
  // // return tail * (1.0f - t) + head * t;

  // float a = std::cos(t * juce::MathConstants<float>::halfPi);
  // float b = std::sin(t * juce::MathConstants<float>::halfPi);
  // return tail * a + head * b;
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
  const float scale = commonVoiceSate->pgWaveformEnvelopeScale.load();
  float v = getEnvelopeValueAtPhase(commonVoiceSate->pgWaveformEnvelopeData, phase, scale);

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

float PulsarSynthVoice::getFmLfoValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->fmLfoData, phase, commonVoiceSate->fmLfoScale.load()); }

float PulsarSynthVoice::getAmLfoValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->amLfoData, phase, commonVoiceSate->amLfoScale.load()); }

// db to gain
float PulsarSynthVoice::getOutputGain() { return commonVoiceSate->outputGainParam == nullptr ? 1 : std::pow(10.0f, commonVoiceSate->outputGainParam->load() / 20.0f); }

// 当 PRF 较高（例如 > 50Hz）：粒子紧密相连，人耳无法分辨单个粒子，它们融合在一起，形成一个连续的音调或厚重的 Pad 音色。
// 此时 PRF 调制会让声音产生类似“合唱”或“相位移动”的流动感。
// 当 PRF 较低（例如 < 20Hz）：人耳能清晰地听到“哒、哒、哒”的单个脉冲。
// 此时 PRF 调制就变成了节奏调制（Rhythmic Modulation），你可以用它来做极其复杂的、非线性的 Glitch 节奏或序列。
float PulsarSynthVoice::getPerlinModulation() const {
  float noiseVal = perlinNoise.noise(static_cast<float>(perlinTime * 2.0));
  return 1.0f + noiseVal * 0.05f; // 5% smooth PRF variation
}
