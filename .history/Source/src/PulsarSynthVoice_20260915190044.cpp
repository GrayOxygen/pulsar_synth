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

// ============================ envelope时间线去锁定 ============================
// 各调制包络的时间线速率比(基准=envIndex/56，每个pulsar period前进一步)：
// 全部共用同一速率时，amp/fm/pan/cluster包络+波表扫描锁死在同一个2048-period循环上
// 同相重复——听感=一个固定loop很快重现。黄金比例族的无理数比让各条包络以互不成
// 整数比的周期循环，组合状态几乎永不重演，声音沿着包络持续演化下去
namespace {
constexpr double kAmEnvTimelineRate = 1.0;
constexpr double kFmEnvTimelineRate = 0.6180339887;      // 1/φ
constexpr double kPanEnvTimelineRate = 0.7861513777;     // √(1/φ)
constexpr double kClusterEnvTimelineRate = 1.2720196495; // √φ
// 包络"一圈时长"与画布点数解耦：ENVELOPE_SIZE只是曲线分辨率(查表有线性插值，
// 任意点数都平滑)；一圈演化时长=kEnvLoopPeriods个pulsar period，想更慢的演化调大它即可，
// 不需要动画布点数。wrap上限取kEnvLoopPeriods的大整数倍：rate=1.0的amp包络wrap无缝，
// 各条无理数速率时间线在整个区间内自由发展、不被逐圈硬重置(否则全部同相回零=整体loop)
constexpr double kEnvLoopPeriods = 1024.0;
constexpr int kEnvIndexWrap = static_cast<int>(kEnvLoopPeriods) * 4096;
inline float envTimelinePhase(int envIndex, double rate) { return static_cast<float>(std::fmod(static_cast<double>(envIndex) * rate / kEnvLoopPeriods, 1.0)); }
} // namespace

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
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(envTimelinePhase(envIndex, kClusterEnvTimelineRate), commonVoiceSate->dutyCycleClusterDepthParam->load());
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
  snapShot.dutyCycleCluster = getCurrentDutyCycleCluster(envTimelinePhase(envIndex, kClusterEnvTimelineRate), commonVoiceSate->dutyCycleClusterDepthParam->load());
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
      granular.resetGrains();
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
    float l = 0.0f, r = 0.0f;
    this->processSample(l, r);
    // 立体声：左右声道来自granular引擎的per-grain pan渲染；mono输出时折叠回中间
    if (commonVoiceSate->pulseBuffer.getNumChannels() > 1) {
      commonVoiceSate->pulseBuffer.setSample(0, i, l);
      commonVoiceSate->pulseBuffer.setSample(1, i, r);
    } else {
      commonVoiceSate->pulseBuffer.setSample(0, i, 0.5f * (l + r));
    }
  }

  // cache the output gain once per block instead of recomputing std::pow per sample/channel
  const float outputGain = getOutputGain();
  const int numChannels = outputBuffer.getNumChannels();

  // pan包络：按envelope时间线(envIndex逐pulsar前进)取当前pan，equal-power增益；
  // 一个block内pan计一次(block通常远短于包络变化尺度)，避免逐sample查表
  const float pan = getCurrentPan(envTimelinePhase(envIndex, kPanEnvTimelineRate), commonVoiceSate->panDepthParam->load());
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
      // FastMathApproximations::tanh是有理逼近，|x|>~4.9后发散(输出>1甚至爆增长)：
      // mask gap后归一化增益过冲的瞬间样本若超出有效域会变成刺耳的碎裂声，先clamp再削波
      const float s = juce::dsp::FastMathApproximations::tanh(juce::jlimit(-4.0f, 4.0f, pulseReadPtr[i])) * outputGain * panGain;
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

  // pulsar extension
  commonVoiceSate->harmonicsParam = apvts.getRawParameterValue(why::ParameterID::harmonics);
  commonVoiceSate->unisonDetuneParam = apvts.getRawParameterValue(why::ParameterID::unisonDetune);
  commonVoiceSate->unisonWidthParam = apvts.getRawParameterValue(why::ParameterID::unisonWidth);

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

  // pulsar extension
  if (parameterID == why::ParameterID::harmonics && commonVoiceSate->harmonicsParam != nullptr) {
    commonVoiceSate->harmonicsParam->store(newValue);
  }
  if (parameterID == why::ParameterID::unisonDetune && commonVoiceSate->unisonDetuneParam != nullptr) {
    commonVoiceSate->unisonDetuneParam->store(newValue);
  }
  if (parameterID == why::ParameterID::unisonWidth && commonVoiceSate->unisonWidthParam != nullptr) {
    commonVoiceSate->unisonWidthParam->store(newValue);
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
    if (snapShot.dutyCycleCluster != getCurrentDutyCycleCluster(envTimelinePhase(envIndex, kClusterEnvTimelineRate), commonVoiceSate->dutyCycleClusterDepthParam->load())) {
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
  double fmLatch = calcFmLfoInterpolation(static_cast<float>(granular.getLfoPhase()), commonVoiceSate->fmLfoDepthParam->load());

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
    // envIndex在放大的上限wrap(2048的整数倍)：各条无理数速率的包络时间线在整个
    // kEnvIndexWrap区间内自由发展，组合模式不会每2048个period就整体重演一次
    // envIndex *= (1 + static_cast<int>(std::sin(fmLatch * juce::MathConstants<float>::twoPi) * EnvelopeCanvas::ENVELOPE_SIZE));
    // envIndex = envIndex * (1 + fmLatch * EnvelopeCanvas::ENVELOPE_SIZE);
    envIndex++;
    if (envIndex >= kEnvIndexWrap)
      envIndex -= kEnvIndexWrap;
    if (envIndex < 0) {
      envIndex += kEnvIndexWrap;
    }
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
    // envIndex *= (1 + static_cast<int>(std::sin(fmLatch * juce::MathConstants<float>::twoPi) * EnvelopeCanvas::ENVELOPE_SIZE));
    if (envIndex >= kEnvIndexWrap)
      envIndex -= kEnvIndexWrap;
    if (envIndex < 0) {
      envIndex += kEnvIndexWrap;
    }
    break;
  }
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

// writes the stereo sample pair for the current sample position
void PulsarSynthVoice::processSample(float &outL, float &outR) {
  // Advance Perlin noise time for smooth modulation
  perlinTime += (1.0f / getSampleRate());

  // spawnGrain在trigger瞬间Latch当前值冻结进grain
  granular.advanceLfo();

  // Enter next stage
  if (hasPassedSampleNumInsideTrain >= currentStateDurationSampleNum) {
    bool wasEnterNextTrain = enterNextTrain.load();
    if (wasEnterNextTrain) {
      // Use the latest train configuration from the interface as the next train configuration，正式进入下一个train
      resetTrain();
      enterNextTrain = false;
    }
    // latch方式推进bpm包络：每个stage走完一次，推进到下一个点，新bpm决定后续stage时长
    advanceBpmEnvelope();
    // 刷新duty cycle ratio, duty cycle cluseter
    refreshPulsarInSnapShot();
    changeStage();
  }

  // calc sample
  calSampleByState(true, false, outL, outR);

  hasPassedSampleNumInsideTrain++;
}

void PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask, float &outL, float &outR) {
  juce::ignoreUnused(passMaskFlag, existMask);
  calcActualPulse(outL, outR);
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

// (trigger * mask)：在trigger瞬间用mask决定是否spawn；被遮蔽则本period无grain，
// 但绝不影响之前grain的尾巴(applyMaskString按stage index查表，无状态，可在此安全调用)
void PulsarSynthVoice::maskGatedSpawn() {
  bool maskPass = true;
  bool maskExist = false;
  // train silence不走mask
  if (currentState != why::PulsarStateEnum::InterTrainSilence) {
    mask(maskPass, maskExist);
  }

  // 注意类型：之前的int r = nextFloat()被截断成永远为0，3×谐波每次都以amp=0 spawn
  double r = juce::Random::getSystemRandom().nextFloat();

  if (maskPass) {
    double fmLatch = calcFmLfoInterpolation(static_cast<float>(granular.getLfoPhase()), commonVoiceSate->fmLfoDepthParam->load());
    juce::ignoreUnused(r);

    spawnGrain(snapShot.fundamentalFreq, 1.0f, 1.0f, true, 1.0f);

    // 谐波pulsar层：同一trigger时刻在2x/3x/4x基频spawn相干grain层，电平按1/n递减(自然谐波衰减)，
    // fund=false不覆盖voice级波表扫描/LFO速率latch——与基频同相叠加变厚而不是echo
    const float harmonics = commonVoiceSate->harmonicsParam != nullptr ? commonVoiceSate->harmonicsParam->load() : 0.0f;
    if (harmonics > 1.0e-3f) {
      // 分音呼吸走amplitude而不是length：length的per-trigger随机跳变(白噪r/可为0的fmLatch)
      // 会让谐波整周期地突然变长/消失=flicker；每个谐波一条独立慢速perlin曲线(≈0.1Hz量级，
      // spawn时Latch)，各分音以不同速率缓慢起伏——像自然乐器的分音呼吸，无dropout
      const float h2 = 0.5f + 0.5f * perlinNoise.noise(static_cast<float>(perlinTime * 0.13 + 31.0));
      const float h3 = 0.5f + 0.5f * perlinNoise.noise(static_cast<float>(perlinTime * 0.17 + 67.0));
      const float h4 = 0.5f + 0.5f * perlinNoise.noise(static_cast<float>(perlinTime * 0.11 + 139.0));
      // fmLatch作为亮度倾斜：LFO越大高次谐波越亮(激励越强频谱越亮的自然联动)，
      // 但夹在0~1且只缩放不置零——LFO过零时高次分音变暗而不是消失
      const float bright = juce::jlimit(0.0f, 1.0f, 0.5f + 0.5f * static_cast<float>(fmLatch));
      // 0.35底+0.65呼吸深度：分音永远在场(无硬切)，只在-9dB~0dB间缓慢起伏
      spawnGrain(snapShot.fundamentalFreq * 2.0, 0.5 * harmonics / 2.0 * (0.35f + 0.65f * h2), 1.0f, false, 1.0f);
      spawnGrain(snapShot.fundamentalFreq * 3.0, 0.5 * harmonics / 3.0 * (0.35f + 0.65f * h3) * bright, 1.0f, false, 1.0f);
    }

    // unison失谐层：基频的±detune cents拷贝按width左右展开(per-grain equal-power pan)，
    // cents级失谐产生缓慢拍频=supersaw式厚度/立体声宽度，不会像大失谐那样粗糙
    const float unisonDetune = commonVoiceSate->unisonDetuneParam != nullptr ? commonVoiceSate->unisonDetuneParam->load() : 0.0f;
    if (unisonDetune > 0.01f) {
      const float width = commonVoiceSate->unisonWidthParam != nullptr ? commonVoiceSate->unisonWidthParam->load() : 0.0f;
      spawnGrain(snapShot.fundamentalFreq, 0.7f, 1.0f, false, 1.0f, -unisonDetune, -width);
      spawnGrain(snapShot.fundamentalFreq, 0.7f, 1.0f, false, 1.0f, +unisonDetune, +width);
    }
  }
}

// spawn grain
void PulsarSynthVoice::spawnGrain(double baseFreq, double amFactor, double length, bool fund, double rateMod, double extraDetuneCents, float pan) {
  if (length <= 0)
    return;

  // double r = juce::Random::getSystemRandom().nextFloat();
  // int active = getCellActiveCount();

  // amp/fm各走自己的无理数速率时间线：两条包络的循环周期互不成整数比，组合不重复
  float amp = calcAmEnvelopeInterpolation(envTimelinePhase(envIndex, kAmEnvTimelineRate), commonVoiceSate->amEnvDepthParam->load());
  double fmSlice = calcFmEnvelopeInterpolation(envTimelinePhase(envIndex, kFmEnvTimelineRate), commonVoiceSate->fmEnvelopeDepthParam->load());
  amp *= static_cast<float>(amFactor);
  if (amp <= 1.0e-4f)
    return;
  
  const double periodSamples = getSampleRate() / baseFreq;
  // 取整到整数个pulsar period
  int durCycles = static_cast<int>(snapShot.dutyCycleCluster) * length;
  int playbackSamples = static_cast<int>(std::round(periodSamples * durCycles));

  // 规定：
  //    一个 pulsaret 恰好播放一个完整 waveform cycle
  //    所以 waveform cycle = d
  const double scanFreq = getSampleRate() / playbackSamples;
  if (!(scanFreq > 0.0))
    return;
  const double phaseInc = 1.0 / playbackSamples;

  // 失谐(extraDetuneCents，非unison grain为0)：
  // double detuneRatio = std::pow(2.0, extraDetuneCents / 1200.0);
  // double phaseInc = cycleSamples > 0.0 ? detuneRatio / cycleSamples : 0.0;
  // double phaseInc = cycleSamples > 0.0 ? 1.0f / cycleSamples : 0.0;
  // 重叠补偿在spawn时烘焙进per-grain amp(而不是渲染端按活跃数做master归一化)：
  // 预期重叠数=grain时长/发射周期；COLA下N个hann窗和≈N/2，除以overlap/2后
  // 重叠稳态响度与单grain一致。烘焙式补偿下grain生灭(尤其mask的不规则生灭)
  // 不再调制总线增益——mask边界零咔哒，空洞只是窗和的平滑凹陷
  // const double periodSamples = getSampleRate() / baseFreq;
  // const double overlapEst = periodSamples > 0.0 ? static_cast<double>(playbackSamples) / periodSamples : 1.0;
  // amp = static_cast<float>(amp / std::max(1.0, overlapEst * 0.5));
  // double phaseInc = cycleSamples > 0.0 ? 1.0f / cycleSamples : 0.0;
  // double phaseInc = playbackSamples > 0.0 ? 1.0f / playbackSamples : 0.0;

  //  只提供trigger时刻算好的参数
  GranularEngine::SpawnParams params;
  params.startPhase = 0;
  params.phaseInc = phaseInc;
  params.playbackSamples = playbackSamples;
  params.delaySamples = 0;
  params.amp = amp;
  params.baseFreq = baseFreq;
  params.fund = fund;
  params.sampleRate = getSampleRate();
  params.pan = pan;
  granular.spawn(params);
}

// granular核心引擎完成所有grain的overlap-add渲染与归一化(立体声：per-grain pan)
void PulsarSynthVoice::calcActualPulse(float &outL, float &outR) { granular.renderSample(getSampleRate(), outL, outR); }

float PulsarSynthVoice::getCurrentDutyCycleCluster(float phase, float depth) {
  float envelopeValue = getDutyCycleClusterEnvelopeValueAtPhase(phase);
  return depth * envelopeValue;
}

// pan包络：画布0~1映射到双极性-1(左)~+1(右)，0.5=居中；depth缩放偏离中心的幅度
float PulsarSynthVoice::getCurrentPan(float phase, float depth) {
  float envelopeValue = getPanEnvelopeValueAtPhase(phase);
  return juce::jlimit(-1.0f, 1.0f, (envelopeValue * 2.0f - 1.0f) * depth);
}

// depth作为调制混合(dry/wet)而不是直接乘：depth=1完全按包络，depth->0收敛回1(原始音高)。
// 之前value*depth在depth小时把scanFreq压到基频的~1%——次声频率的grain=听感上突然静音
float PulsarSynthVoice::calcFmEnvelopeInterpolation(float phase, float depth) { return 1.0f + depth * (getFmEnvelopeValueAtPhase(phase) - 1.0f); }

float PulsarSynthVoice::calcAmEnvelopeInterpolation(float phase, float depth) {
  // depth<=0或不使用包络时返回中性1.0(不调制)而不是0：
  // 之前返回0会让spawnGrain以amp=0丢弃所有grain——输出整体突然静音
  if (depth <= 0.0f || !commonVoiceSate->useAmpEnvelope.load()) {
    return 1.0f;
  }
  float envelopeValue = getAmpEnvelopeValueAtPhase(phase);
  // depth作为dry/wet混合：1=完全按包络，0=无调制，中间线性过渡
  return 1.0f + depth * (envelopeValue - 1.0f);
}

float PulsarSynthVoice::calcFmLfoInterpolation(float phase, float depth) {
  // float semitone = getFmLfoValueAtPhase(phase);
  // // float semitone = juce::jmap(env, 0.01f, 1.0f, -1.0f, 1.0f);
  // float ratio = std::pow(2.0f, semitone * depth / 9600.0f);
  // return ratio;
  // return (0.5f + (2.0f * getFmLfoValueAtPhase(phase) * depth - 1.0f) / 2.0f) * getPerlinModulation(); // [0, 1]
  return getFmLfoValueAtPhase(phase) * depth;
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

float PulsarSynthVoice::getFmLfoValueAtPhase(float phase) const { return getEnvelopeValueAtPhase(commonVoiceSate->fmLfoData, phase, commonVoiceSate->fmLfoScale.load()); }

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
