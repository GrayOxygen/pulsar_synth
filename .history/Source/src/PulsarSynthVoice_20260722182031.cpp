//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/PulsarSynthVoice.h"
#include "../include/CommonVoiceSate.h"
#include "../include/PulsarSynthSound.h"
#include <algorithm>
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
  // 阶段时长用clamp到≤1的ratio：发射周期(spawn间隔)永远=一个pulsar period，不随ratio>1拉长；
  // 未clamp的ratio只决定grain时长(spawnGrain)，ratio>1时grain跨多个period自然overlap(NuPG式)
  float stageRatio = std::min(1.0f, snapShot.dutyCycleRatio);
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, stageRatio * snapShot.pulsarPeriodTime * sampleRate);
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
  // 阶段时长用clamp到≤1的ratio(与saveSnapShot一致)：发射周期固定=一个pulsar period，
  // silence钳到≥1 sample保证periodsDoneInTrain计数正常(负duration不计数会让train永远走不完)；
  // 未clamp的ratio只决定grain时长，ratio>1时grain跨多个period自然overlap(NuPG式)
  float stageRatio = std::min(1.0f, snapShot.dutyCycleRatio);
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, stageRatio * snapShot.pulsarPeriodTime * sr);
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
    float result = this->processSample();
    commonVoiceSate->pulseBuffer.setSample(0, i, result);
    if (commonVoiceSate->pulseBuffer.getNumChannels() > 1)
      commonVoiceSate->pulseBuffer.setSample(1, i, result);
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
  periodsDoneInTrain = 0;
  // pulsarStageIndexInTrainDutyCycle = 0;
  // envIndex = 0;
  firstTrain = true;
  // 不清空活跃的waveform grains，让它们自然播放完毕，避免声音突然中断
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
    // 一个完整pulsar period(pulse+silence)结束；刚进入train时的初始态duration为0，不计数
    if (currentStateDurationSampleNum > 0)
      periodsDoneInTrain++;

    // train duty cycle部分走完(已完成trainDutyCycleLen个pulsar period)
    if (periodsDoneInTrain >= static_cast<int>(snapShot.trainDutyCycleLenParam)) {
      if (snapShot.trainSilenceParam > 0.0f) {
        // 进入inter-train silence，走完后(下个case)开启新train
        currentState = why::PulsarStateEnum::InterTrainSilence;
        currentStateDurationSampleNum = static_cast<int>(snapShot.interTrainSilenceSamples);
        endPosInTrainSamples += currentStateDurationSampleNum;
        break;
      }
      // 无train silence：直接开启新train
      periodsDoneInTrain = 0;
      endPosInTrainSamples = 0;
      firstTrain = false;
    }
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    endPosInTrainSamples += static_cast<int>(snapShot.pulsarDutyCycleSamples);
    pulsarStageIndexInTrainDutyCycle++;
    // spawn a waveform grain: capture fmModulation at trigger time as fixed phase increment
    // NuPG式trigger-gating：mask只决定是否触发本period的grain(丢trigger)，不掐已在播放的grain
    maskGatedSpawn();
    break;
  case why::PulsarStateEnum::InterTrainSilence: // train silence finished，开启新train
    firstTrain = false;
    periodsDoneInTrain = 0;
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    endPosInTrainSamples = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    pulsarStageIndexInTrainDutyCycle++;
    maskGatedSpawn();
    break;
  }
}

// NuPG式(trigger * mask)：在trigger瞬间用mask决定是否spawn；被遮蔽则本period无grain，
// 但绝不影响之前grain的尾巴(applyMaskString按stage index查表，无状态，可在此安全调用)
void PulsarSynthVoice::maskGatedSpawn() {
  bool maskPass = true;
  bool maskExist = false;
  mask(maskPass, maskExist);
  if (maskPass) {
    spawnGrain();
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
float PulsarSynthVoice::processSample() {
  // Advance Perlin noise time for smooth modulation
  perlinTime += (1.0 / getSampleRate());

  // NuPG式自由运行调制器：全局LFO相位逐sample推进(含silence期间)，不随trigger重置；
  // spawnGrain在trigger瞬间Latch当前值冻结进grain
  lfoPhase += lfoPhaseInc;
  if (lfoPhase >= 1.0)
    lfoPhase -= 1.0;

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

    while (currentStateDurationSampleNum <= 0) {
      changeStage();
    }

    // pulsarAdsr.noteOff();
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
  // float newPulsarFreq;
  // calcNewPulsarFreq(snapShot.dutyCycleRatio, newPulsarFreq, silenceToPulseFlag, static_cast<int>(snapShot.dutyCycleCluster));

  // Trigger the release phase: Within the application duration, there should still be room to apply the release; otherwise, it will not be triggered
  if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && (currentStateDurationSampleNum - hasPassedSampleNumInsideTrain) <= pulsarAdsrParams.release * getSampleRate()) {
    pulsarAdsr.noteOff();
    isTriggeredReleaseFlag = true;
  }

  // calc sample
  float s = calSampleByState(passMaskFlag, existMask);

  hasPassedSampleNumInsideTrain++;
  return s;
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask) {
  juce::ignoreUnused(passMaskFlag, existMask);
  // NuPG式渲染路径：mask已在trigger处gate(maskGatedSpawn，不spawn即无声)，这里永远只求和活跃grain：
  // - 不再按stage硬切/乘stage fade窗：它们会在每个period边界把跨period的overlap grain尾巴
  //   集体掐0或重新拉起(尤其ratio≥1时silence阶段只块1 sample，每个period产生1-sample dropout毛刺)
  // - grain自带Hann窗保证生灭平滑，stage边界对已发声grain完全透明(同GrainBuf)
  return calcActualPulse();
}

int PulsarSynthVoice::getCellActiveCount() {
  uint8_t next = 0; // 一维数组celluar 8 bits
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
  // 增加一点pitch抖动
  double caPitchOffset = perlinMod; //  perlin
  float scale = 0.9f + 0.2f * static_cast<float>(active) / 8.0f;
  double envPhase = 1.0f * envIndex / 2048;

  // 获取实时envelope; 每个sample，对应一个envelope点，确定fm，am，dc ratio，dc cluster，然后用确定的freq扫描waveform
  // 基准频率与pulsar period绑定：一个pulsar period恰好扫完一个完整波形周期，与bpm/train长度无关
  double baseFreq = snapShot.fundamentalFreq;
  // 高PRF时衰减per-grain随机音高抖动：脉冲密集(>20Hz)融合成音高后，周期抖动会把线状谱抹成噪声谱(noise感)；
  // 低PRF时保留全部抖动作为有机感
  double prf = baseFreq;
  double jitterAtten = prf > 20.0 ? 20.0 / prf : 1.0;
  scale = 1.0f + static_cast<float>((scale - 1.0f) * jitterAtten);
  caPitchOffset = 1.0 + (caPitchOffset - 1.0) * jitterAtten;
  double fmMult = calcFmEnvelopeInterpolation(envPhase, commonVoiceSate->fmEnvelopeDepthParam->load()) * caPitchOffset;

  // NuPG式Latch：自由运行的全局LFO在trigger瞬间采样一次，值冻结进本grain
  double fmLatch = calcFmLfoInterpolation(static_cast<float>(lfoPhase), commonVoiceSate->fmLfoDepthParam->load());
  double amLatch = calcAmLfoInterpolation(static_cast<float>(lfoPhase), commonVoiceSate->amLfoDepthParam->load());

  // NuPG式grain模型(GrainBuf: pos=0, dur=dilation/formantFreq)：调制后的rate决定单个波形周期时长
  // (rate升高->周期变短)，包络时长=dilation×cluster×ratio个周期；grain生灭由首尾为0的hann窗保证平滑，
  // 无需整数周期闭合
  double rate = fmMult * fmLatch;
  // 单周期时长由clamp到≤1的ratio决定(音高不随ratio>1下降)：ratio<1与旧行为一致(周期=duty段)；
  // ratio>1时不拉长周期，而是增加grain内的完整周期数——grain播得更久、跨period自然overlap，音高不变
  double grainBaseSamples = std::min(1.0f, snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime * getSampleRate();
  double cycleSamples = rate > 0.0 ? grainBaseSamples / rate : grainBaseSamples;    // 单个波形周期的样本数
  double overlapMult = std::max(1.0, static_cast<double>(snapShot.dutyCycleRatio)); // ratio>1的部分转成周期数倍增
  // NuPG式envelope dilation(GrainBuf: dur = dilation/ffreq, rate不变)：
  // grain寿命 = dilation × cluster × ratio 个波形周期，可为小数——不再强制整数周期；
  // 波形以固定rate自由推进/wrap，dilation<1时被包络中途掐断(hann窗首尾为0，截断处无click，
  // 时域越短频域越宽 -> formant带宽变宽)；dilation>1时一个包络下装更多周期
  double dilation = static_cast<double>(commonVoiceSate->envelopeDilation.load());
  double durCycles = std::max(0.05, static_cast<double>(snapShot.dutyCycleCluster) * overlapMult);
  int samples = std::max(1, static_cast<int>(std::round(cycleSamples * durCycles)));
  double phaseInc = cycleSamples > 0.0 ? 1.0 / cycleSamples : 0.0; // 纯播放速率，与包络时长解耦
  int delaySamples = static_cast<int>((currentStateDurationSampleNum / 64.0f) * (1.0f * active > 5 ? active : 0));
  delaySamples = 0;
  // int samples = static_cast<int>(currentStateDurationSampleNum * snapShot.dutyCycleCluster);
  // int delaySamples = 0;
  float amp = calcAmEnvelopeInterpolation(envPhase, commonVoiceSate->amEnvDepthParam->load());

  // wavetable scanning：wtPos绑定train相位——一个train的duty cycle部分(trainDutyCycleLen个period)正好扫完整个波表；
  // grain内wtPos逐sample连续滑动(每个period前进1/trainDutyCycleLen)，滑到下一个grain的spawn落点，帧morph无台阶；
  // 下一个train从0重新扫 
  // float wtPos = static_cast<float>(periodsDoneInTrain) / snapShot.trainDutyCycleLenParam;
  // float wtPosInc = static_cast<float>(snapShot.fundamentalFreq / (snapShot.trainDutyCycleLenParam * getSampleRate()));

  // // wavetable scanning：扫描位置由train相位决定，后续每个pulsar依次扫到不同的波形帧
  //   // wtPos在grain生命周期内按envPhase时间线连续滑动(每次spawn前进1/2048、spawn间隔=1个period)，
  //   // 而非冻结在spawn时刻的值：帧morph变成连续glide，相邻pulsar间不会出现帧位置台阶
  //   float wtPos = static_cast<float>(envPhase);
  //   float wtPosInc = static_cast<float>(snapShot.fundamentalFreq / (2048.0 * getSampleRate()));
  // // 2048个pulsar走完一整个waveform frames
  float wtPos = static_cast<float>(envPhase);
  float wtPosInc = static_cast<float>(snapShot.fundamentalFreq * (1 + active) / (2048.0 * getSampleRate()));
  
  // NuPG式：调制器速率跟随频率(envM = ffreq * envMul * 2048/sr)——LFO频率 = grain波形频率(ffreq)，
  // envMul=1时一个波形周期对应一个LFO周期；cycleSamples = sr/ffreq，故inc = 1/cycleSamples。
  // 相位仍全局自由运行(processSample逐sample推进)，trigger时Latch；fm/amLatch已在上方采样
  // lfoPhaseInc = cycleSamples > 0.0 ? 1.0 / cycleSamples : 0.0;
  lfoPhaseInc = cycleSamples > 0.0 ? 1.0 / cycleSamples : 0.0;
  amp *= static_cast<float>(amLatch); // 整个grain用冻结的幅度

  // NuPG式(GrainBuf pos:0)：每个grain固定从波形头部(phase=0)开始，播整数个完整周期后结束，
  // 首尾天然同点，不再需要相位延续。
  // NuPG式池管理(GrainBuf maxGrains语义)：只写入空闲slot；池满时丢弃新grain(丢新保旧)，
  // 绝不覆盖掐0正在播放的grain，避免高重叠时的硬切click
  int slot = -1;
  for (int i = 0; i < MAX_WAVEFORM_GRAINS; ++i) {
    int idx = (waveformGrainWriteIdx + i) % MAX_WAVEFORM_GRAINS;
    if (!waveformGrains[idx].active) {
      slot = idx;
      break;
    }
  }
  if (slot >= 0) {
    waveformGrains[slot] = {0.0f, phaseInc, true, samples, samples, delaySamples, scale, 0, amp, wtPos, wtPosInc};
    waveformGrainWriteIdx = (slot + 1) % MAX_WAVEFORM_GRAINS;
  }

  envIndex++;
  if (envIndex >= 2048)
    envIndex = envIndex - 2048;
}

float PulsarSynthVoice::calcActualPulse() {
  // formant frequency, emmission frequency完全无关
  double waveformSample;
  double grainSum = 0.0f;
  double s = 0;

  for (auto &g : waveformGrains) {
    if (!g.active || g.delaySamples-- > 0)
      continue;

    double raw = getWaveformEnvelopeValueAtPhase(g.phase, g.wtPos);
    raw = juce::jlimit(-1.0, 1.0, raw);

    // wavetable帧位置逐sample连续滑动(与envPhase时间线同速)，wrap回0处两端grain窗恰好接近0，无台阶
    g.wtPos += g.wtPosInc;
    if (g.wtPos >= 1.0f)
      g.wtPos -= 1.0f;

    // 跨pulsar duty cycle window：per-grain Hann窗，50%重叠时窗和恒定(COLA)，首尾为0，grain生灭无click
    double completeWindow = hannWindow(static_cast<float>(g.windowPhase));
    g.windowPhase += 1.0 / static_cast<double>(g.totalSamples);
    if (g.windowPhase > 1.0)
      g.windowPhase = 1.0;

    // NuPG式：LFO已在spawn时Latch进g.amp和g.phaseInc，grain内rate/amp恒定，不再逐sample调制
    // 不再乘全局stage窗(windowInCurrentStage)：它会在每个pulse边界把所有grain一起掐0，破坏overlap平滑；stage边界的防click由getPulseFadeGain负责
    grainSum += raw * completeWindow * g.amp; // am modulation(已Latch)
    g.phase += g.phaseInc;                    // fm modulation(已Latch进phaseInc)
    if (g.phase >= 1.0f)
      g.phase -= 1;

    // 如果播够一个pulsar duty cycle
    g.remainSamples--;
    if (g.remainSamples <= 0) {
      g.active = false;
    }
  }
  // NuPG式：重叠grain直接相加不归一化(GrainBuf mul:0.9)，重叠即增益、越叠越厚(formant共振感)；
  // 电平由输出端limiter/用户增益兜底
  waveformSample = grainSum * 0.9;
  s = waveformSample * pulsarAdsr.getNextSample();
  return s;
}

float PulsarSynthVoice::getCurrentDutyCycleCluster(float phase, float depth) {
  if (depth <= 0.0f) {
    return 1.0f;
  }

  float envelopeValue = getDutyCycleClusterEnvelopeValueAtPhase(phase);
  return depth * envelopeValue;
}

float PulsarSynthVoice::getCurrentDutyCycleRatio(float phase, float depth) {
  float envelopeValue = getDutyCycleRatioEnvelopeValueAtPhase(phase);
  // float yMin = commonVoiceSate->dutyCycleRatioEnvelopeYMin.load();
  // float yMax = commonVoiceSate->dutyCycleRatioEnvelopeYMax.load();
  // float modFactor = juce::jmap(envelopeValue, yMin, yMax, 0.01f, 1.0f);
  return depth * envelopeValue;
}

float PulsarSynthVoice::calcFmEnvelopeInterpolation(float phase, float depth) { return getFmEnvelopeValueAtPhase(phase) * depth; }

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
  float semitone = getFmLfoValueAtPhase(phase);
  // float semitone = juce::jmap(env, 0.01f, 1.0f, -1.0f, 1.0f);
  float ratio = std::pow(2.0f, semitone * depth / 12.0f);
  return ratio;
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

float PulsarSynthVoice::getWaveformEnvelopeValueAtPhase(float phase, float wtPos) const {
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
