//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/PulsarSynthVoice.h"
#include "../include/CommonVoiceSate.h"
#include "../include/PulsarSynthSound.h"
#include "LfoPage.h"
#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include <random>
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
  snapShot.bpm = getCurrentBpm();
  snapShot.trainDutyCycleLenParam = commonVoiceSate->trainDutyCycleLenParam->load();
  snapShot.trainSilenceParam = commonVoiceSate->trainSilenceParam->load();
  snapShot.trainLenParam = commonVoiceSate->trainLenParam->load();

  snapShot.trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  snapShot.trainTime = snapShot.trainLenParam * snapShot.trainLenBlock;
  snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainSilenceParam + snapShot.trainDutyCycleLenParam);
  // snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainDutyCycleLenParam);
  snapShot.fundamentalFreq = 1.0 / snapShot.pulsarPeriodTime;
  snapShot.trainSilenceTime = snapShot.trainSilenceParam * snapShot.pulsarPeriodTime;
  snapShot.trainDutyCycleTime = snapShot.trainDutyCycleLenParam * snapShot.pulsarPeriodTime;

  // train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
  snapShot.trainPeriodTime = snapShot.trainDutyCycleTime + snapShot.trainSilenceTime;

  // 原duty cycle ratio包络已改作pan包络：ratio固定为1(duty阶段=整个period，无intra silence)
  snapShot.dutyCycleRatio = 1.0f;
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load());
  snapShot.dutyCycleTime = snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime;
  snapShot.pulsarSilenceTime = (1 - snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime;

  snapShot.pulsarFreq = 1.0 / snapShot.dutyCycleTime;
  float sampleRate = getSampleRate();
  // 阶段时长用clamp到≤1的ratio：发射周期(spawn间隔)永远=一个pulsar period，不随ratio>1拉长
  float stageRatio = std::min(1.0f, snapShot.dutyCycleRatio);
  snapShot.pulsarDutyCycleSamples = std::max(1.0f, stageRatio * snapShot.pulsarPeriodTime * sampleRate);
  snapShot.pulsarIntraSilenceSamples = std::max(1.0f, snapShot.pulsarPeriodTime * sampleRate - snapShot.pulsarDutyCycleSamples);
  snapShot.interTrainSilenceSamples = std::max(1.0f, snapShot.trainSilenceTime * sampleRate);
  snapShot.trainDutyCycleSamples = std::max(1.0f, snapShot.trainDutyCycleTime * sampleRate);
}

// 只更新快照中的pulsar数据，因为pulsar period长度已定，但duty cycle raito, duty cycle cluster可以变化，每个pulsar阶段切换都要更新
void PulsarSynthVoice::refreshPulsarInSnapShot() {
  // 原duty cycle ratio包络已改作pan包络：ratio固定为1(与saveSnapShot一致)
  snapShot.dutyCycleRatio = 1.0f;
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load());
  snapShot.dutyCycleTime = snapShot.dutyCycleRatio * snapShot.pulsarPeriodTime;
  snapShot.pulsarSilenceTime = (1 - snapShot.dutyCycleRatio) * snapShot.pulsarPeriodTime;
  snapShot.pulsarFreq = 1.0 / snapShot.dutyCycleTime;

  float sr = getSampleRate();
  // 阶段时长用clamp到≤1的ratio(与saveSnapShot一致)：发射周期固定=一个pulsar period
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
      bpmEnvIndex = 0;
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

  // pan包络：按envelope时间线(envIndex逐pulsar前进)取当前pan，equal-power增益；
  // 一个block内pan计一次(block通常远短于包络变化尺度)，避免逐sample查表
  const float pan = getCurrentPan(1.0f * envIndex / 2048, commonVoiceSate->panDepthParam->load());
  const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // -1~1 -> 0~pi/2
  const float gainL = std::cos(panAngle);
  const float gainR = std::sin(panAngle);

  for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++) {
    int i = sampleIndex - startSample;
    for (int chan = 0; chan < numChannels; chan++) {
      const auto *pulseReadPtr = commonVoiceSate->pulseBuffer.getReadPointer(chan);
      // fast tanh soft-clip approximation instead of std::tanh
      // const float s = dcBlocker.process(juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i])) * outputGain;
      const float panGain = numChannels > 1 ? (chan == 0 ? gainL : gainR) : 1.0f;
      const float s = juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i]) * outputGain * panGain;
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
  // 清空活跃的waveform grains，让它们自然播放完毕，避免声音突然中断
  // waveformGrainWriteIdx = 0;
  setEnterNextTrain(true);
}

void PulsarSynthVoice::resetTrain() {
  saveSnapShot();
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
}

void PulsarSynthVoice::mappingParams(const juce::AudioProcessorValueTreeState &apvts) {
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
  commonVoiceSate->panDepthParam = apvts.getRawParameterValue(why::ParameterID::panDepth);
  commonVoiceSate->dutyCycleClusterDepthParam = apvts.getRawParameterValue(why::ParameterID::dutyCycleClusterDepth);
  commonVoiceSate->fmLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::fmLfoDepth);
  commonVoiceSate->amLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::amLfoDepth);

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
  if (parameterID == why::ParameterID::panDepth) {
    commonVoiceSate->panDepthParam->store(newValue);
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
  if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen || parameterID == why::ParameterID::trainLen) {
    // if train params were changed
    if (snapShot.trainSilenceParam != commonVoiceSate->trainSilenceParam->load() || snapShot.trainLenParam != commonVoiceSate->trainLenParam->load() ||
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
  // panDepth只影响输出声像(processBlockSamples逐sample读)，不需要刷新snapshot
  if (parameterID == why::ParameterID::dutyCycleClusterDepth) {
    if (snapShot.dutyCycleCluster != getCurrentDutyCycleCluster(1.0f * envIndex / 2048, commonVoiceSate->dutyCycleClusterDepthParam->load())) {
      refreshPulsarInSnapShot();
    }
  }
}

void PulsarSynthVoice::reloadPreset(juce::AudioProcessorValueTreeState &apvts) {
  mappingParams(apvts);
  // init train
  saveSnapShot();
}

// latch当前bpm包络点的值，限幅到合理BPM范围
float PulsarSynthVoice::getCurrentBpm() const {
  float bpm = commonVoiceSate->bpmEnvelopeData[static_cast<size_t>(bpmEnvIndex)];
  return juce::jlimit(commonVoiceSate->bpmEnvelopeYMin.load(), commonVoiceSate->bpmEnvelopeYMax.load(), bpm);
}

// 用最新latch的bpm重算快照中的train时长相关字段（其余train参数仍保持快照latch语义）
void PulsarSynthVoice::refreshBpmInSnapShot() {
  snapShot.bpm = getCurrentBpm();
  snapShot.trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  snapShot.trainTime = snapShot.trainLenParam * snapShot.trainLenBlock;
  snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainSilenceParam + snapShot.trainDutyCycleLenParam);
  // snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainDutyCycleLenParam);
  snapShot.fundamentalFreq = 1.0 / snapShot.pulsarPeriodTime;
  snapShot.trainSilenceTime = snapShot.trainSilenceParam * snapShot.pulsarPeriodTime;
  snapShot.trainDutyCycleTime = snapShot.trainDutyCycleLenParam * snapShot.pulsarPeriodTime;
  snapShot.trainPeriodTime = snapShot.trainDutyCycleTime + snapShot.trainSilenceTime;

  float sampleRate = getSampleRate();
  snapShot.interTrainSilenceSamples = std::max(1.0f, snapShot.trainSilenceTime * sampleRate);
  snapShot.trainDutyCycleSamples = std::max(1.0f, snapShot.trainDutyCycleTime * sampleRate);
}

// latch方式推进bpm包络：每个stage走完一次，trigger推进到下一个点，新bpm latch到下一个stage
void PulsarSynthVoice::advanceBpmEnvelope() {
  bpmEnvIndex++;
  if (bpmEnvIndex >= EnvelopeCanvas::ENVELOPE_SIZE)
    bpmEnvIndex -= EnvelopeCanvas::ENVELOPE_SIZE;

  if (snapShot.bpm != getCurrentBpm())
    refreshBpmInSnapShot();
}

bool PulsarSynthVoice::applyMaskString(const std::string &maskStr, bool &existMask, bool &maskPassFlag) {
  if (!maskStr.empty() && periodsDoneCounter > 0) {
    existMask = true;
    int index = (periodsDoneCounter - 1) % static_cast<int>(maskStr.size());
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
    periodsDoneCounter++;

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
    maskGatedSpawn();
    envIndex++;
    if (envIndex >= 2048)
      envIndex = envIndex - 2048;
    break;
  case why::PulsarStateEnum::InterTrainSilence: // train silence finished，开启新train
    firstTrain = false;
    periodsDoneInTrain = 0;
    currentState = why::PulsarStateEnum::Pulse;
    currentStateDurationSampleNum = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    endPosInTrainSamples = static_cast<int>(snapShot.pulsarDutyCycleSamples);
    pulsarStageIndexInTrainDutyCycle++;
    maskGatedSpawn();
    envIndex++;
    if (envIndex >= 2048)
      envIndex = envIndex - 2048;
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
  perlinTime += (1.0f / getSampleRate());

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
      // envIndex++;
      // if (envIndex >= 2048)
      //   envIndex = envIndex - 2048;
    }
    // latch方式推进bpm包络：每个stage走完一次，推进到下一个点，新bpm决定后续stage时长
    advanceBpmEnvelope();
    // 刷新duty cycle ratio, duty cycle cluseter
    refreshPulsarInSnapShot();
    changeStage();

    // ADSR已改为per-grain形状(spawn时Latch进grain，跨grain全寿命)：
    // 不再每stage reset全局ADSR——它会在每个边界把所有overlap尾巴集体拉零再attack，产生周期性硬切
    isTriggeredReleaseFlag = false;
  }

  // bool passMaskFlag = true; // the mask of current sample is 1
  // bool existMask = false;

  // // train silence不走mask
  // if (currentState != why::PulsarStateEnum::InterTrainSilence) {
  //   mask(passMaskFlag, existMask);
  // }

  // It used to be pulsar silence and now it has become pulse. To confirm the final frequency of pulsar and the adsr ratio value
  // bool silenceToPulseFlag = currentState == why::PulsarStateEnum::IntraSilence && existMask && passMaskFlag;

  // The frequency after pulse duty cycle modulation will affect the length of the pulse
  // float newPulsarFreq;
  // calcNewPulsarFreq(snapShot.dutyCycleRatio, newPulsarFreq, silenceToPulseFlag, static_cast<int>(snapShot.dutyCycleCluster));

  // calc sample
  // float s = calSampleByState(passMaskFlag, existMask);
  float s = calSampleByState(true, false);

  hasPassedSampleNumInsideTrain++;
  return s;
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask) {
  juce::ignoreUnused(passMaskFlag, existMask);
  // 渲染路径：mask已在trigger处gate(maskGatedSpawn，不spawn即无声)，只求和活跃grain：
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

// (trigger * mask)：在trigger瞬间用mask决定是否spawn；被遮蔽则本period无grain，
// 但绝不影响之前grain的尾巴(applyMaskString按stage index查表，无状态，可在此安全调用)
void PulsarSynthVoice::maskGatedSpawn() {
  bool maskPass = true;
  bool maskExist = false;
  // train silence不走mask
  if (currentState != why::PulsarStateEnum::InterTrainSilence) {
    mask(maskPass, maskExist);
  }

  int r = juce::Random::getSystemRandom().nextFloat();

  if (maskPass) {
    // NuPG: rate_One * (1 + Latch(LFSaw(ffreq * fmRatio, ...), trigger))
    // trigger瞬间Latch自由运行LFO的当前值，冻结进本grain：
    // fmLatch(音高ratio，等效NuPG的(1+mod))作用于grain播放速率，amLatch作用于grain振幅
    double fmLatch = calcFmLfoInterpolation(static_cast<float>(lfoPhase), commonVoiceSate->fmLfoDepthParam->load());
    double amLatch = calcAmLfoInterpolation(static_cast<float>(lfoPhase), commonVoiceSate->amLfoDepthParam->load());

    spawnGrain(snapShot.fundamentalFreq, 1.0f, 1.0f, true, 1.0f);

    float driftPitch = perlinNoise.noise(static_cast<float>(perlinTime * 0.31 + 17.0)); // ~0.3Hz慢漂
    // float freq = snapShot.fundamentalFreq * std::pow(2.0, static_cast<double>(driftPitch) * 12.0 / 1200.0);
    // spawnGrain(snapShot.fundamentalFreq * std::pow(2.0, static_cast<double>(driftPitch) * 12.0 / 1200.0), //
    //            r, (fmLatch == 0 ? 0 : 1.0f / fmLatch), false,                                             //
    //            1.0f                                                                                       //
    // );
    // spawnGrain(freq, 1.0f, (fmLatch == 0 ? 0 : 1.0f / fmLatch), false, (std::cos(amLatch * r) * std::cos(amLatch * r) + std::sin(amLatch) * std::sin(amLatch)));
    double phase = std::fmod(static_cast<float>(fmLatch) / juce::MathConstants<float>::twoPi + driftPitch, 1.0f);
    double saw = 2.0f * phase - 1.0f;
    double triangle = 1.0f - 4.0f * std::abs(phase - 0.5f);
    double sine = std::sin(phase);
    int active = getCellActiveCount();

    // spawnGrain(snapShot.fundamentalFreq * 2.0f, amLatch * r * active / 8.0f, 0.75f, false, triangle);
    // spawnGrain(snapShot.fundamentalFreq * 3.0f, 1.0f * r * active / 8.0f, 0.75f, false, sine);
    // spawnGrain(snapShot.fundamentalFreq * 4.0f, amLatch * active / 8.0f, 0.5f, false, saw);
  }
}

// 基准频率与pulsar period绑定：一个pulsar period恰好per trigger
void PulsarSynthVoice::spawnGrain(double baseFreq, double amFactor, double length, bool fund, double rateMod) {
  if (length <= 0)
    return;

  double r = juce::Random::getSystemRandom().nextFloat();
  int active = getCellActiveCount();
  double envPhase = 1.0f * envIndex / 2048.0f;

  // 自由运行的全局LFO在trigger瞬间采样一次，值冻结进本grain
  double fmLatch = calcFmLfoInterpolation(static_cast<float>(lfoPhase), commonVoiceSate->fmLfoDepthParam->load());
  float amp = calcAmEnvelopeInterpolation(envPhase, commonVoiceSate->amEnvDepthParam->load());
  double fmSlice = calcFmEnvelopeInterpolation(envPhase, commonVoiceSate->fmEnvelopeDepthParam->load());
  amp *= static_cast<float>(amFactor);
  double scanFreq = baseFreq * fmSlice;

  // 有机漂移(spawn时Latch，grain内恒定不破坏相位相干)：三条独立慢速perlin曲线，
  // 分别微调音高(±12 cents)、音量(±25%)、波表位置(±3%)——参数不动时音色也会自动缓慢演化
  float driftPitch = perlinNoise.noise(static_cast<float>(perlinTime * 0.31 + 17.0)); // ~0.3Hz慢漂
  float driftAmp = perlinNoise.noise(static_cast<float>(perlinTime * 0.53 + 113.0));  // 独立offset避免三条曲线相关
  float driftPhase = perlinNoise.noise(static_cast<float>(perlinTime * 0.01 + 3.0));
  // float driftWt = perlinNoise.noise(static_cast<float>(perlinTime * 0.19 + 271.0));
  // scanFreq *= std::pow(2.0, static_cast<double>(driftPitch) * 12.0 / 1200.0); // ±12 cents微失谐漂移
  amp *= 1.0f + driftAmp * 0.05f; // ±5%音量呼吸感
  float freqModPhase = std::fmod(fmLatch, 1.0f);
  float triangle = 1.0f - 4.0f * std::abs(freqModPhase - 0.5f);
  // scanFreq *= (1.0f + std::sin(baseFreq * fmLatch) * driftPitch);
  double cycleSamples = std::round(getSampleRate() / scanFreq);

  // 相位相干：grain准时在period边界触发、从phase 0开始——重叠时同相叠加变厚而不是echo；
  // 之前的随机delay(最多~8ms)和随机起始相位会把重叠grain变成错开的delay tap，产生echo感
  int delaySamples = 0;
  double startPhase = 0;
  delaySamples = r < 0.1 ? static_cast<int>(active + driftPhase * snapShot.pulsarDutyCycleSamples) : 0;
  startPhase = r < 0.7 ? static_cast<double>(active / 8.0f + driftPhase) : 0.0;

  double durCycles = static_cast<double>(snapShot.dutyCycleCluster);
  if (durCycles <= 0 || cycleSamples <= 0) {
    return;
  }
  int playbackSamples = std::max(1, static_cast<int>(std::round(cycleSamples * durCycles)));
  // 纯播放速率(与包络时长解耦)：所有grain同一rate才能同相叠加；
  // 之前的(1.0f + active < 3 ? 0 : active/3.0f)因优先级实际是((1+active)<3)?0:active/3：
  // rate为0(波形冻结成闷响)或跳到active/3倍音高，失谐拷贝叠加=echo/chorus浑浊感
  // NuPG rate语义(GrainBuf)：rateMod只改播放速率(音高)，不改grain时长——
  // playbackSamples仍由未调制的cycleSamples决定，phaseInc乘上trigger时Latch的调制值
  double phaseInc = cycleSamples > 0.0 ? 1.0f / cycleSamples : 0.0;

  // wavetable scanning：only latches the scan rate on fundamental triggers — one full table sweep per 2048 pulsar periods
  // (matches envPhase timeline), direction sign preserved so ping-pong isn't reset:
  if (fund && getSampleRate() > 0.0)
    wtScanInc = (wtScanInc < 0.0 ? -1.0 : 1.0) * baseFreq / (2048.0 * getSampleRate());
  float wtPos = static_cast<float>(wtScanPos);

  // follow the scan freq
  lfoPhaseInc = phaseInc;

  // 池满时丢弃新grain(丢新保旧)，避免高重叠时的硬切click
  int slot = -1;
  for (int i = 0; i < MAX_WAVEFORM_GRAINS; ++i) {
    int idx = (waveformGrainWriteIdx + i) % MAX_WAVEFORM_GRAINS;
    if (!waveformGrains[idx].active) {
      slot = idx;
      break;
    }
  }
  if (slot >= 0) {
    waveformGrains[slot] = {startPhase, phaseInc, lfoPhase, lfoPhaseInc, true, playbackSamples, playbackSamples, delaySamples, 1.0f, 0, amp, static_cast<float>(baseFreq), wtPos, 0.0f};
    waveformGrainWriteIdx = (slot + 1) % MAX_WAVEFORM_GRAINS;
  }
}

float PulsarSynthVoice::calcActualPulse() {
  double grainSum = 0.0;
  int activeCount = 0;

  for (auto &g : waveformGrains) {
    if (!g.active || g.delaySamples-- > 0 || g.totalSamples <= 0)
      continue;
    activeCount++;
    // 所有grain读同一个voice级扫描位置：重叠grain帧位置完全相干，crossfade平滑无台阶
    double raw = getWaveformEnvelopeValueAtPhase(g.phase, static_cast<float>(wtScanPos));
    raw = juce::jlimit(-1.0, 1.0, raw);

    // 寿命窗从整段hann改为Tukey(首尾10%淡化+中段平顶)：hann×ADSR把每个grain挤成窄鼓包，
    double completeWindow = cycleEdgeWindow(static_cast<float>(g.windowPhase));
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

    double pulsaretEnvelope = calcAmLfoInterpolation(static_cast<float>(g.phase), commonVoiceSate->amLfoDepthParam->load());
    double adsrWindow = grainAdsrShape(static_cast<float>(g.phase), adsrA, adsrD, adsrS, adsrR);

    g.windowPhase += 1.0 / static_cast<double>(g.totalSamples);
    if (g.windowPhase > 1.0)
      g.windowPhase = 1.0;

    // LFO已在spawn时Latch进g.amp和g.phaseInc，grain内rate/amp恒定，不再逐sample调制
    // 不再乘全局stage窗(windowInCurrentStage)：它会在每个pulse边界把所有grain一起掐0，破坏overlap平滑；stage边界的防click由getPulseFadeGain负责
    // per-grain ADSR窗：作用于grain整个寿命(windowPhase 0~1)，首尾为0防click；
    // 注意必须用windowPhase而不是g.phase——g.phase每个波形周期循环0~1，会变成基频斩波器
    grainSum += raw * g.amp * pulsaretEnvelope * adsrWindow * completeWindow;
    // 相位只按Latch的rate推进：逐sample用perlin/LFO/ratio晃相位会让各grain音高各自漂移、
    // 互相抵消(washy)；有机感由spawn时的fm/am Latch和包络负责
    g.phase += g.phaseInc;
    if (g.phase >= 1.0f) {
      g.phase -= 1;
    }

    // 如果播够一个pulsar duty cycle
    g.remainSamples--;
    if (g.remainSamples <= 0) {
      g.active = false;
    }
  }
  // voice级扫描位置逐sample推进；ping-pong反弹代替wrap：到达边界时往回滑而不是从末帧跳回首帧，
  // wrap处的音色突变会产生毛刺——反弹让morph轨迹连续来回扫描
  wtScanPos += wtScanInc;
  if (wtScanPos >= 1.0) {
    wtScanPos = 2.0 - wtScanPos;
    wtScanInc = -wtScanInc;
  } else if (wtScanPos < 0.0) {
    wtScanPos = -wtScanPos;
    wtScanInc = -wtScanInc;
  }

  // 归一化增益用平滑后的活跃grain数：grain生灭瞬间activeCount阶跃会让所有其他grain电平突跳(zipper/click)，
  // 一阶低通(~10ms)让增益连续过渡；activeCount==0时冻结，避免下个grain出生时增益从错误值滑动
  if (activeCount > 0) {
    const float alpha = 1.0f - std::exp(-1.0f / (0.01f * static_cast<float>(getSampleRate())));
    smoothedActiveCount += (static_cast<float>(activeCount) - smoothedActiveCount) * alpha;
    if (smoothedActiveCount < 1.0f)
      smoothedActiveCount = 1.0f;
  }
  return activeCount <= 0 ? 0.0f : static_cast<float>(grainSum / std::sqrt(static_cast<double>(smoothedActiveCount)));
  // 重叠grain直接相加不归一化(GrainBuf mul:0.9)，重叠即增益、越叠越厚(formant共振感)；
  // 电平由输出端limiter/用户增益兜底
  // return static_cast<float>(grainSum) * 0.9f;
}

float PulsarSynthVoice::getCurrentDutyCycleCluster(float phase, float depth) {
  float envelopeValue = getDutyCycleClusterEnvelopeValueAtPhase(phase);
  return depth * envelopeValue;
}

// pan包络：画布0~1映射到双极性-1(左)~+1(右)，0.5=居中；depth缩放偏离中心的幅度
float PulsarSynthVoice::getCurrentPan(float phase, float depth) {
  float envelopeValue = getPanEnvelopeValueAtPhase(phase);
  return juce::jlimit(-1.0f, 1.0f, (envelopeValue * 2.0f - 1.0f) * depth);
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
  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  phase = std::fmod(std::abs(phase), 1.0f);
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;
  float val1 = data[std::min(index, size - 1)] * scale;
  float val2 = data[std::min(index + 1, size - 1)] * scale;
  return val1 + frac * (val2 - val1);

  // constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  // phase -= std::floor(phase); // 比 fmod 更适合 phase
  // float indexF = phase * size;
  // int i0 = (int)indexF;
  // float frac = indexF - i0;
  // i0 %= size;
  // int i1 = (i0 + 1) % size;
  // float a = data[static_cast<size_t>(i0)] * scale;
  // float b = data[static_cast<size_t>(i1)] * scale;
  // return a + (b - a) * frac;

  // phase = std::fmod(std::abs(phase), 1.0f);

  // constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;
  // constexpr float crossfadeRatio = 0.3f; // 最后3%进行crossfade

  // auto sampleAt = [&](float p) {
  //   p = std::fmod(std::abs(p), 1.0f);

  //   float indexF = p * (size - 1);
  //   int index = static_cast<int>(indexF);
  //   float frac = indexF - index;

  //   float v1 = data[static_cast<size_t>(index)];
  //   float v2 = data[static_cast<size_t>(index + 1) % size];

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

float PulsarSynthVoice::getPanEnvelopeValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->panEnvelopeData, phase, commonVoiceSate->panEnvelopeScale.load()); }

float PulsarSynthVoice::getDutyCycleClusterEnvelopeValueAtPhase(float phase) const {
  return getEnvelopeValueAtPhase(commonVoiceSate->dutyCycleClusterEnvelopeData, phase, commonVoiceSate->dutyCycleClusterEnvelopeScale.load());
}

float PulsarSynthVoice::getWaveformEnvelopeValueAtPhase(float phase, float wtPos) {
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
  return 1.0f + noiseVal * 0.02f; // 3% smooth PRF variation
}
