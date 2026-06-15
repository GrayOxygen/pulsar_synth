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

//================================================overwrite methods================================================
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

// ==================================customize method ：train相关参数不要load，每次输出时，只使用snapshot，保证前后计算统一不出错====================================
// In auto mode, bypass startnote and directly call to play the sample
void PulsarSynthVoice::saveSnapShot() {
  snapShot.bpm = why::bpm;
  snapShot.trainDutyCycleLenParam = commonVoiceSate->trainDutyCycleLenParam->load();
  snapShot.trainSilenceParam = commonVoiceSate->trainSilenceParam->load();
  snapShot.trainLenParam = commonVoiceSate->trainLenParam->load();
  // snapShot.pulsarDutyCycleRatioParam = commonVoiceSate->pulsarDutyCycleRatioParam->load();
  // snapShot.pulsarWaveformParam = commonVoiceSate->pulsarWaveformParam->load();
  // snapShot.pulsarDutyCycleClusterLenParam = commonVoiceSate->pulsarDutyCycleClusterLenParam->load();
  // snapShot.attackParam = commonVoiceSate->attackParam->load();
  // snapShot.decayParam = commonVoiceSate->decayParam->load();
  // snapShot.sustainParam = commonVoiceSate->sustainParam->load();
  // snapShot.releaseParam = commonVoiceSate->releaseParam->load();

  snapShot.trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  snapShot.trainTime = snapShot.trainLenParam * snapShot.trainLenBlock;
  snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainSilenceParam + snapShot.trainDutyCycleLenParam);
  snapShot.fundamentalFreq = 1.0 / snapShot.pulsarPeriodTime;
  snapShot.trainSilenceTime = snapShot.trainSilenceParam * snapShot.pulsarPeriodTime;
  snapShot.trainDutyCycleTime = snapShot.trainDutyCycleLenParam * snapShot.pulsarPeriodTime;

  // train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
  snapShot.trainPeriodTime = snapShot.trainDutyCycleTime + snapShot.trainSilenceTime;

  // snapShot.pulsarDutyCycleSamples = snapShot.pulsarDutyCycleRatioParam * snapShot.pulsarPeriodTime * getSampleRate();
  // snapShot.pulsarIntraSilenceSamples = snapShot.pulsarPeriodTime * getSampleRate() - snapShot.pulsarDutyCycleSamples;
  snapShot.interTrainSilenceSamples = snapShot.trainSilenceTime * getSampleRate();
  snapShot.trainDutyCycleSamples = snapShot.trainDutyCycleTime * getSampleRate();
}

void PulsarSynthVoice::saveNonTrainParams() {
  // snapShot.pulsarWaveformParam = commonVoiceSate->pulsarWaveformParam->load();
  // snapShot.pulsarDutyCycleClusterLenParam = commonVoiceSate->pulsarDutyCycleClusterLenParam->load();
  // snapShot.attackParam = commonVoiceSate->attackParam->load();
  // snapShot.decayParam = commonVoiceSate->decayParam->load();
  // snapShot.sustainParam = commonVoiceSate->sustainParam->load();
  // snapShot.releaseParam = commonVoiceSate->releaseParam->load();
}

void PulsarSynthVoice::refreshSnapShot(int trainDurationLen, int trainIntervalSilenceLen, int trainLen) {
  // 使用新的train值构建snapshot：bpm, newTrainDurationLen, newTrainIntervalSilenceLen, newTrainLen
  snapShot.bpm = why::bpm;
  snapShot.trainDutyCycleLenParam = trainDurationLen;
  snapShot.trainSilenceParam = trainIntervalSilenceLen;
  snapShot.trainLenParam = trainLen;
  // snapShot.pulsarDutyCycleRatioParam = commonVoiceSate->pulsarDutyCycleRatioParam->load();
  // snapShot.pulsarWaveformParam = commonVoiceSate->pulsarWaveformParam->load();
  // snapShot.pulsarDutyCycleClusterLenParam = commonVoiceSate->pulsarDutyCycleClusterLenParam->load();
  // snapShot.attackParam = commonVoiceSate->attackParam->load();
  // snapShot.decayParam = commonVoiceSate->decayParam->load();
  // snapShot.sustainParam = commonVoiceSate->sustainParam->load();
  // snapShot.releaseParam = commonVoiceSate->releaseParam->load();

  snapShot.trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  snapShot.trainTime = snapShot.trainLenParam * snapShot.trainLenBlock;
  snapShot.pulsarPeriodTime = snapShot.trainTime / (snapShot.trainSilenceParam + snapShot.trainDutyCycleLenParam);
  snapShot.fundamentalFreq = 1.0 / snapShot.pulsarPeriodTime;
  snapShot.trainSilenceTime = snapShot.trainSilenceParam * snapShot.pulsarPeriodTime;
  snapShot.trainDutyCycleTime = snapShot.trainDutyCycleLenParam * snapShot.pulsarPeriodTime;

  // train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
  snapShot.trainPeriodTime = snapShot.trainDutyCycleTime + snapShot.trainSilenceTime;

  // snapShot.pulsarDutyCycleSamples = snapShot.pulsarDutyCycleRatioParam * snapShot.pulsarPeriodTime * getSampleRate();
  // snapShot.pulsarIntraSilenceSamples = snapShot.pulsarPeriodTime * getSampleRate() - snapShot.pulsarDutyCycleSamples;
  snapShot.interTrainSilenceSamples = snapShot.trainSilenceTime * getSampleRate();
  snapShot.trainDutyCycleSamples = snapShot.trainDutyCycleTime * getSampleRate();
}

void PulsarSynthVoice::renderNextBlockDirectly(juce::AudioSampleBuffer &outputBuffer, juce::AudioPlayHead *audioPlayHead, int startSample, int numSamples) {
  // Control the playback and stop of the DAW to trigger the start and stop of the pulsar process respectively
  // 刷新非train参数（waveform/ADSR/cluster），train timing参数只在train边界更新
  saveNonTrainParams();

  if (audioPlayHead != nullptr && audioPlayHead->getPosition()) {
    bool isNowPlaying = audioPlayHead->getPosition()->getIsPlaying();

    if (isNowPlaying && !wasPlayingLastFrame) {
      // Transport just started
      isActive = true;
      soundOffWhenSwitchPlayMode = false;

      resetTrain(commonVoiceSate->trainDutyCycleLenParam->load(), commonVoiceSate->trainSilenceParam->load(), true, commonVoiceSate->trainLenParam->load());
    }

    if (!isNowPlaying && wasPlayingLastFrame) {
      // Transport just stopped
      isActive = false;
    }

    wasPlayingLastFrame = isNowPlaying;
  }

  if (isActive && !soundOffWhenSwitchPlayMode) {
    processSampleWithConvolution(outputBuffer, startSample, numSamples);
  }
}

void PulsarSynthVoice::processSampleWithConvolution(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) {
  commonVoiceSate->pulseBuffer.clear();
  if (commonVoiceSate->pulseBuffer.getNumSamples() < numSamples || commonVoiceSate->pulseBuffer.getNumChannels() < outputBuffer.getNumChannels()) {
    commonVoiceSate->pulseBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, true, true); // 自动释放并重新分配
  }
  auto *leftWritePtr = commonVoiceSate->pulseBuffer.getWritePointer(0);
  auto *rightWritePtr = commonVoiceSate->pulseBuffer.getWritePointer(1);

  for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++) {
    int i = sampleIndex - startSample;
    float currentSample = this->processSample();
    leftWritePtr[i] = currentSample;
    rightWritePtr[i] = currentSample;
  }

  const int impulseMode = static_cast<int>(commonVoiceSate->impulseSwitchParam->load());
  if (impulseMode == static_cast<int>(why::ImpulseSwitchEnum::Template)) { // template file as IR,  pulseBuffer as source
    commonVoiceSate->convolutionResource->processSample(commonVoiceSate->pulseBuffer);
  } else if (impulseMode == static_cast<int>(why::ImpulseSwitchEnum::Sample)) { // sample source convolved  with pulsar as IR
                                                                                // 用完整的累积 buffer 作为 IR 进行卷积   TODO 暂时改回来，直接用当前buffer做卷积

    commonVoiceSate->convolutionResource->processSampleSourceWithPulsarIr(commonVoiceSate->pulseBuffer, commonVoiceSate->pulseBuffer);
    // const int irTargetSize = juce::jlimit(512, 16384, (int)(trainPeriodTime * getSampleRate()));

    // if (commonVoiceSate->pulsarIrAccTargetSize != irTargetSize) {
    //   commonVoiceSate->pulsarIrAccumulatorBuffer.setSize(2, irTargetSize, false, true, true);
    //   commonVoiceSate->pulsarIrAccumulatorBuffer.clear();
    //   commonVoiceSate->pulsarIrAccWritePos = 0;
    //   commonVoiceSate->pulsarIrAccumulatedSamples = 0;
    //   commonVoiceSate->pulsarIrReady = false;
    //   commonVoiceSate->pulsarIrAccTargetSize = irTargetSize;
    // }

    // // 循环滚动写入IR Buffer
    // for (int ch = 0; ch < 2; ++ch) {
    //   auto *accPtr = commonVoiceSate->pulsarIrAccumulatorBuffer.getWritePointer(ch);
    //   const auto *srcPtr = commonVoiceSate->pulseBuffer.getReadPointer(ch);
    //   for (int i = 0; i < numSamples; ++i) {
    //     accPtr[commonVoiceSate->pulsarIrAccWritePos] = srcPtr[i];
    //     commonVoiceSate->pulsarIrAccWritePos = (commonVoiceSate->pulsarIrAccWritePos + 1) % irTargetSize;
    //   }
    // }

    // commonVoiceSate->pulsarIrAccumulatedSamples += numSamples;
    // if (commonVoiceSate->pulsarIrAccumulatedSamples >= irTargetSize) {
    //   commonVoiceSate->pulsarIrReady = true;
    // }

    // const int effectiveIRLength = juce::jmin(commonVoiceSate->pulsarIrAccumulatedSamples, irTargetSize);
    // if (commonVoiceSate->pulsarIrReady && commonVoiceSate->irReloadCounter >= 2048) {
    //   commonVoiceSate->irReloadCounter = 0;

    //   commonVoiceSate->convolutionResource->buildLinearIR(commonVoiceSate->pulsarIrAccumulatorBuffer, commonVoiceSate->pulsarIrAccWritePos, commonVoiceSate->pulsarLinearIrBuffer,
    //   effectiveIRLength);
    // }

    // commonVoiceSate->irReloadCounter += numSamples;
    // commonVoiceSate->convolutionResource->processSampleSourceWithPulsarIr(commonVoiceSate->pulsarLinearIrBuffer, commonVoiceSate->pulseBuffer);
  }

  // cache the output gain once per block instead of recomputing std::pow per sample/channel
  const float outputGain = getOutputGain();
  const int numChannels = outputBuffer.getNumChannels();

  for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++) {
    int i = sampleIndex - startSample;
    for (int chan = 0; chan < numChannels; chan++) {
      const auto *pulseReadPtr = commonVoiceSate->pulseBuffer.getReadPointer(chan);
      const float s = pulseReadPtr[i] * outputGain;
      // fast tanh soft-clip approximation instead of std::tanh
      // const float s = juce::dsp::FastMathApproximations::tanh(pulseReadPtr[i] * 1.5f) * outputGain;
      // float channelFactor = 1.0f;
      float channelFactor = (chan % 2 == 0) ? 0.73f : 0.80f; // 增加stereo双声道差异化
      outputBuffer.addSample(chan, sampleIndex, s * channelFactor);
    }
  }
}

void PulsarSynthVoice::setPulsarSilence(float dutyCylceRatio) {
  // pulsarDutyCycleRatio = dutyCylceRatio;
  // pulsarSilenceTime = pulsarPeriodTime * (1 - snapShot.pulsarDutyCycleRatioParam);
}

void PulsarSynthVoice::resetTrainInitialSate() {
  currentState = why::PulsarStateEnum::IntraSilence;
  hasPassedSampleNumInsideTrain = -999;
  trainCounter = 0.0;
  currentStateDurationSampleNum = 0;
  endPosInTrainSamples = 0; // the end position of current state
  pulsarStageIndexInTrainDutyCycle = 0;
  pulsaretPhase = 0.0f;
}

// 等待最近的pulsar silence或train interval silence结束再reset并进入新的train
void PulsarSynthVoice::changeToNewTrainAfterPulsarPeriodOrTrainEnd(bool bpmChangedFlag) {
  // int trainIntervalSilenceLen = static_cast<int>(snapShot.trainSilenceParam);
  // int trainDutyCycleLen = static_cast<int>(snapShot.trainDutyCycleLenParam);
  // int trainLen = static_cast<int>(snapShot.trainLenParam);
  // int oldTrainIntervalSilenceLen = this->previousTrainSilenceNum;
  // int oldTrainDutyCycleLen = this->previousTrainLen;
  // int oldTrainLen = this->previousTrainDutyCycleNum;

  // if (trainIntervalSilenceLen + trainDutyCycleLen <= 0) {
  //   return;
  // }

  // // Change the train trajectory: Reset the current position of the train,
  // // reset it to the new train and then change it to false (updated in
  // // processSample)
  // isTheSameTrainConfig = oldTrainIntervalSilenceLen == trainIntervalSilenceLen && oldTrainDutyCycleLen == trainDutyCycleLen && oldTrainLen == trainLen && !bpmChangedFlag;
  // if (isTheSameTrainConfig) {
  //   return;
  // }

  // this->previousTrainSilenceNum = trainIntervalSilenceLen;
  // this->previousTrainLen = trainLen;
  // this->previousTrainDutyCycleNum = trainDutyCycleLen;

  // changeTrainTrace = true;
  // // new train config
  // newTrainIntervalSilenceLen = trainIntervalSilenceLen;
  // newTrainDurationLen = trainDutyCycleLen;
  // newTrainLen = trainLen;

  changeTrainTrace = true;
  // new train config
  // newTrainIntervalSilenceLen = trainIntervalSilenceLen;
  // newTrainDurationLen = trainDutyCycleLen;
  // newTrainLen = trainLen;
}

// Will not wait for build a new train directly, and changeToNewTrainAfterPulsarPeriodOrTrainEnd can wait
void PulsarSynthVoice::resetTrain(int newTrainDurationLen, int newTrainIntervalSilenceLen, float isLoop, int newTrainLen) {
  refreshSnapShot(newTrainDurationLen, newTrainIntervalSilenceLen, newTrainLen);
  // // If the fundamental freq changes, it must be returned to phase; otherwise, the waveform will shift
  // pulsaretPhase = 0.0;
  // use new adsr
  refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * snapShot.pulsarPeriodTime);

  // Rebuild the train config
  // initTrain(durationLen, intervalSilenceLen, isLoop, trainLen);
  // Restore the initial state of train
  // pulsar行走过程中用来判断位置的相关samples，即长度即位置
  // resetTrainRelatedSamples4Location();
  // train initializes the state
  resetTrainInitialSate();
}

// stochastic mask will not be generated. It will only be generated when the train dutycycle is changed
void PulsarSynthVoice::initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen) {
  // if (intervalSilenceLen + durationLen <= 0) {
  //   return;
  // }
  // newTrainIntervalSilenceLen = intervalSilenceLen;
  // newTrainDurationLen = durationLen;
  // newTrainLen = trainLen;

  // realChangeTrainConfig();
}

void PulsarSynthVoice::realChangeTrainConfig() {

  // // Currently, it is 1/4 beat. train length is how many 1/4 (1 beat) there are
  // trainLenBlock = (60.0 / snapShot.bpm) / why::beatDivision.load();
  // trainTime = newTrainLen * trainLenBlock;
  // pulsarPeriodTime = trainTime / (newTrainIntervalSilenceLen + newTrainDurationLen);
  // setPulsarSilence(snapShot.pulsarDutyCycleRatioParam);
  // fundamentalFreq = 1.0 / pulsarPeriodTime;
  // trainSilenceTime = newTrainIntervalSilenceLen * pulsarPeriodTime;
  // trainDutyCycleTime = newTrainDurationLen * pulsarPeriodTime;

  // // train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
  // trainPeriodTime = trainDutyCycleTime + trainSilenceTime;
  // // If the fundamental freq changes, it must be returned to phase; otherwise, the waveform will shift
  // pulsaretPhase = 0.0;

  // // use new adsr
  // refreshPulsaretAdsr(snapShot.pulsarDutyCycleRatioParam * pulsarPeriodTime);

  // // Recompute the location sample counts here (off the per-sample hot path)
  // resetTrainRelatedSamples4Location();
}

void PulsarSynthVoice::initSynthVoice(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>> &sampleBuffer, juce::AudioPlayHead *audioPlayHead) {
  juce::ignoreUnused(sampleRate, audioPlayHead);
  commonVoiceSate->sampleBuffer = std::move(sampleBuffer);

  // Prepare per-voice LFO modulator with initial frequency
  lfoModulator.prepare(sampleRate, 1.0f);
  saveSnapShot();

  // refreshSnapShot(commonVoiceSate->trainDutyCycleLenParam->load(), commonVoiceSate->trainSilenceParam->load(), commonVoiceSate->trainLenParam->load());
  // initTrain(static_cast<int>(snapShot.trainDutyCycleLenParam), snapShot.trainSilenceParam, true, snapShot.trainLenParam);

  commonVoiceSate->generateStochasticMask();

  // 刷新adsr
  refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * snapShot.pulsarPeriodTime);
}

void PulsarSynthVoice::connectParameters(juce::AudioProcessorValueTreeState &apvts) { mappingParams(apvts); }

void PulsarSynthVoice::mappingParams(const juce::AudioProcessorValueTreeState &apvts) {
  // output
  commonVoiceSate->outputGainParam = apvts.getRawParameterValue(why::ParameterID::outputGain);

  // play mode
  commonVoiceSate->playModeParam = apvts.getRawParameterValue(why::ParameterID::playMode);

  // train
  commonVoiceSate->trainLenParam = apvts.getRawParameterValue(why::ParameterID::trainLen);
  commonVoiceSate->trainDutyCycleLenParam = apvts.getRawParameterValue(why::ParameterID::trainDutyCycleLen);
  commonVoiceSate->trainSilenceParam = apvts.getRawParameterValue(why::ParameterID::trainSilenceLen);

  // this->previousTrainSilenceNum = commonVoiceSate->trainSilenceParam->load();
  // this->previousTrainLen = commonVoiceSate->trainLenParam->load();
  // this->previousTrainDutyCycleNum = commonVoiceSate->trainDutyCycleLenParam->load();

  // pulsar basic info
  commonVoiceSate->pulsarWaveformParam = apvts.getRawParameterValue(why::ParameterID::pulsarWaveform);
  commonVoiceSate->pulsarDutyCycleClusterLenParam = apvts.getRawParameterValue(why::ParameterID::pulsarDutyCycleClusterLen);
  commonVoiceSate->pulsarDutyCycleRatioParam = apvts.getRawParameterValue(why::ParameterID::pulsarDutyCycleRatio);

  // lfo waveform shape + depth
  commonVoiceSate->ampLfoWaveformParam = apvts.getRawParameterValue(why::ParameterID::ampLfoWaveform);
  commonVoiceSate->formantFreqLfoWaveformParam = apvts.getRawParameterValue(why::ParameterID::formantFreqLfoWaveform);
  commonVoiceSate->ampLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::ampLfoDepth);
  commonVoiceSate->formantFreqLfoDepthParam = apvts.getRawParameterValue(why::ParameterID::formantFreqLfoDepth);

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
  if (parameterID == why::ParameterID::pulsarDutyCycleClusterLen) {
    commonVoiceSate->pulsarDutyCycleClusterLenParam->store(newValue);
  }
  if (parameterID == why::ParameterID::pulsarDutyCycleRatio) {
    commonVoiceSate->pulsarDutyCycleRatioParam->store(newValue);
  }

  // lfo waveform shape + depth
  if (parameterID == why::ParameterID::ampLfoWaveform) {
    commonVoiceSate->ampLfoWaveformParam->store(newValue);
    lfoModulator.setAmpWaveform(static_cast<int>(newValue));
  }
  if (parameterID == why::ParameterID::formantFreqLfoWaveform) {
    commonVoiceSate->formantFreqLfoWaveformParam->store(newValue);
    lfoModulator.setFormantWaveform(static_cast<int>(newValue));
  }
  if (parameterID == why::ParameterID::ampLfoDepth) {
    commonVoiceSate->ampLfoDepthParam->store(newValue);
  }
  if (parameterID == why::ParameterID::formantFreqLfoDepth) {
    commonVoiceSate->formantFreqLfoDepthParam->store(newValue);
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
  bool updatedBpm = false;

  if (parameterID == why::ParameterID::bpm) {
    updatedBpm = updateBpmDirectly(newValue);
  }

  // Wait for the next pulsar silence or train interval silence to start a new train，只做标记：可以进入下一个train
  if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen || parameterID == why::ParameterID::trainLen || parameterID == why::ParameterID::bpm) {
    changeToNewTrainAfterPulsarPeriodOrTrainEnd(updatedBpm);

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

  // setPulsarSilence(snapShot.pulsarDutyCycleRatioParam);
  // snapShot.pulsarDutyCycleRatioParam = commonVoiceSate->pulsarDutyCycleRatioParam->load();
  // refreshPulsaretAdsr(snapShot.pulsarDutyCycleRatioParam * pulsarPeriodTime);
  // pulsar ratio / cluster / train changes affect the location sample counts: recompute off the hot path
  // resetTrainRelatedSamples4Location();
}

void PulsarSynthVoice::reloadPreset(juce::AudioProcessorValueTreeState &apvts) {
  mappingParams(apvts);
  saveSnapShot();
  // initTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()), commonVoiceSate->trainSilenceParam->load(), true, commonVoiceSate->trainLenParam->load());

  refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * snapShot.pulsarPeriodTime);
}

void PulsarSynthVoice::refreshPulsaretAdsr(float pulsarDutyCycleTime) {
  // Only push the sample rate to the ADSR when it actually changes (avoid
  // redundant work on the audio thread)
  pulsarAdsr.setSampleRate(getSampleRate());
  // if (adsrSampleRate != sr) {
  //   pulsarAdsr.setSampleRate(sr);
  //   adsrSampleRate = sr;
  // }

  float attack = commonVoiceSate->attackParam->load();
  float decay = commonVoiceSate->decayParam->load();
  float release = commonVoiceSate->releaseParam->load();

  // Control ratio(a+d+r) <= 1.0 cut the redundant part
  // TODO 先不重新设置，使用毫秒数
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

  const float newAttack = pulsarDutyCycleTime * attack;
  const float newDecay = pulsarDutyCycleTime * decay;
  const float newSustain = commonVoiceSate->sustainParam->load();
  const float newRelease = pulsarDutyCycleTime * release;

  // // Skip the (relatively expensive) coefficient recompute when nothing changed. In the per-sample hot path these values are constant within a pulsar stage.
  // if (newAttack == pulsarAdsrParams.attack && newDecay == pulsarAdsrParams.decay && newSustain == pulsarAdsrParams.sustain && newRelease == pulsarAdsrParams.release) {
  //   return;
  // }

  pulsarAdsrParams.attack = newAttack;
  pulsarAdsrParams.decay = newDecay;
  pulsarAdsrParams.sustain = newSustain;
  pulsarAdsrParams.release = newRelease;
  // pulsarAdsrParams.attack = snapShot.attackParam;
  // pulsarAdsrParams.decay = snapShot.decayParam;
  // pulsarAdsrParams.sustain = snapShot.sustainParam;
  // pulsarAdsrParams.release = snapShot.releaseParam;
  pulsarAdsr.setParameters(pulsarAdsrParams);
}

void PulsarSynthVoice::mask(bool &maskPassFlag, bool &existMask) {
  if (commonVoiceSate->maskOption == why::MaskOptionEnum::Off) {
    existMask = false;
    // Do not perform mask processing and return according to the original pulsaret
    maskPassFlag = true;
    return;
  }
  // burst masking
  if (commonVoiceSate->maskOption == why::MaskOptionEnum::BurstMask && !commonVoiceSate->burstMask.empty() && pulsarStageIndexInTrainDutyCycle > 0) {
    existMask = true;
    int index = (pulsarStageIndexInTrainDutyCycle - 1) % static_cast<int>(commonVoiceSate->burstMask.size());
    maskPassFlag = commonVoiceSate->burstMask[index] == '1';
  }
  // euclid masking
  if (commonVoiceSate->maskOption == why::MaskOptionEnum::EuclidMask && !commonVoiceSate->euclids.empty() && pulsarStageIndexInTrainDutyCycle > 0) {
    existMask = true;
    int index = (pulsarStageIndexInTrainDutyCycle - 1) % static_cast<int>(commonVoiceSate->euclids.size());
    maskPassFlag = commonVoiceSate->euclids[index] == '1';
  }
  // stochastic masking
  if (commonVoiceSate->maskOption == why::MaskOptionEnum::StochasticMask && !commonVoiceSate->stochasticMaskStr.empty() && pulsarStageIndexInTrainDutyCycle > 0) {
    existMask = true;
    int index = (pulsarStageIndexInTrainDutyCycle - 1) % static_cast<int>(commonVoiceSate->stochasticMaskStr.size());
    maskPassFlag = commonVoiceSate->stochasticMaskStr[index] == '1';
  }
}

/**
 * intraSilenceSamples: pulsar silence samples
 * interTrainSilenceSamples: silence samples between two trains
 */
void PulsarSynthVoice::changeStage(int pulsarDutyCycleSamples, int intraSilenceSamples, int interTrainSilenceSamples, int trainDutyCycleSamples) {
  hasPassedSampleNumInsideTrain = 0;
  // The silence of the mask may also become a pulse. A pulsar cluster can contain multiple pulsaret, and this is the final smallest phase
  pulsaretPhase = 0.0f;

  switch (currentState) {
  case why::PulsarStateEnum::Pulse: // The duty cycle stage of pulsar has been completed at present
    // Check if there is still time to continue this train
    if (endPosInTrainSamples + intraSilenceSamples <= trainDutyCycleSamples) { // 剩余train duty cycle还能跑完至少1个完整的pulsar silence
      currentState = why::PulsarStateEnum::IntraSilence;
      currentStateDurationSampleNum = intraSilenceSamples;
      // The total duration corresponding to the new state
      endPosInTrainSamples += intraSilenceSamples;

      pulsarStageIndexInTrainDutyCycle++;
    } else { // train is over. Enter the new train and mute
      currentState = why::PulsarStateEnum::InterTrainSilence;
      currentStateDurationSampleNum = interTrainSilenceSamples;
      endPosInTrainSamples = 0;
      pulsarStageIndexInTrainDutyCycle = 0;
    }
    break;
  case why::PulsarStateEnum::IntraSilence:                                          // The silence stage of pulsar has now been completed
    if ((endPosInTrainSamples + pulsarDutyCycleSamples) <= trainDutyCycleSamples) { // train duty cycle长度还能播完至少1个完整的pulsar duty cycle
      currentState = why::PulsarStateEnum::Pulse;
      currentStateDurationSampleNum = pulsarDutyCycleSamples;
      endPosInTrainSamples += pulsarDutyCycleSamples;
      pulsarStageIndexInTrainDutyCycle++;
    } else { // If the remaining time is insufficient to complete the full cycle,  simply end the train
      currentState = why::PulsarStateEnum::InterTrainSilence;
      currentStateDurationSampleNum = interTrainSilenceSamples;
      endPosInTrainSamples = 0;
      pulsarStageIndexInTrainDutyCycle = 0;
    }
    break;
  case why::PulsarStateEnum::InterTrainSilence: // train silence finished
    trainCounter++;
    // Restore to the initial state of train
    currentState = why::PulsarStateEnum::IntraSilence;
    currentStateDurationSampleNum = 0.0f;
    endPosInTrainSamples = 0;
    pulsarStageIndexInTrainDutyCycle = 0;
    break;
  }
}

// modulation will affect the length of the pulse
void PulsarSynthVoice::calcNewPulsarFreq(float pulsarDutyCycleRatio, float &pulsarModFreq, bool silenceToPulseFlag) {
  // 开始的单个pulse频率
  pulsarModFreq = 1.0 / (pulsarDutyCycleRatio * snapShot.pulsarPeriodTime);
  // 当ratio为1时，实际上也不存在silence了，所以无需计算新的freq
  if (silenceToPulseFlag && pulsarDutyCycleRatio < 1.0f) {
    pulsarModFreq = 1.0 / ((1 - pulsarDutyCycleRatio) * snapShot.pulsarPeriodTime);
  }

  // The frequency of the pulse is finally multiplied by duty cycle
  pulsarModFreq = commonVoiceSate->pulsarDutyCycleClusterLenParam->load() * pulsarModFreq;
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask, float pulsarModFreq) {
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
    return calcActualPulse(pulsarModFreq);
  case why::PulsarStateEnum::InterTrainSilence:
    return 0.0f;
  }
  return 0.0f;
}

void PulsarSynthVoice::resetTrainRelatedSamples4Location() {
  // pulsarDutyCycleSamples = snapShot.pulsarDutyCycleRatioParam * pulsarPeriodTime * getSampleRate();
  // pulsarIntraSilenceSamples = pulsarPeriodTime * getSampleRate() - pulsarDutyCycleSamples;
  // interTrainSilenceSamples = trainSilenceTime * getSampleRate();
  // trainDutyCycleSamples = trainDutyCycleTime * getSampleRate();
}

float PulsarSynthVoice::processSample() {
  // once playback, default is loop and can not be updated
  if (!commonVoiceSate->isLoop && trainCounter >= 1) {
    return 0;
  }

  // train长度为0不处理
  if (snapShot.trainDutyCycleTime <= 0) {
    return 0.0;
  }

  // NOTE: resetTrainRelatedSamples4Location() is intentionally NOT called per-sample. The location sample counts only change when the train config
  // pulsar ratio changes, so they are recomputed in realChangeTrainConfig(),  resetTrain() and parameterChanged() instead.
  // State switching: Each time the current state is reached, the next stage will begin
  if (hasPassedSampleNumInsideTrain == -999 || hasPassedSampleNumInsideTrain == currentStateDurationSampleNum) {
    // Enter the latest train: Enter after pulsar silence or train silence ends. Or if both silence are 0, directly enter the new train after pulse state.
    DBG("changeTrainTrace=" + juce::String((int)changeTrainTrace) + " state=" + juce::String((int)currentState) + " trainSilence=" + juce::String(snapShot.trainSilenceParam));
    if (changeTrainTrace && (currentState == why::PulsarStateEnum::IntraSilence || currentState == why::PulsarStateEnum::InterTrainSilence)) {
      // Use the latest train configuration from the interface as the next train configuration，正式进入下一个train
      resetTrain(commonVoiceSate->trainDutyCycleLenParam->load(), commonVoiceSate->trainSilenceParam->load(), commonVoiceSate->isLoop, commonVoiceSate->trainLenParam->load());
      changeTrainTrace = false;
    }

    float realTimePularDutyCycleSamples = commonVoiceSate->pulsarDutyCycleRatioParam->load() * snapShot.pulsarPeriodTime * getSampleRate();
    float realTimePularSilenceSamples = snapShot.pulsarPeriodTime * getSampleRate() - realTimePularDutyCycleSamples;
    changeStage(realTimePularDutyCycleSamples, realTimePularSilenceSamples, snapShot.interTrainSilenceSamples, snapShot.trainDutyCycleSamples);

    // If the length of a certain state is 0 samples, it means that the process of the current process should (must) automatically jump to the next stage.
    // Because as the stages change, there will definitely be parts where the stage length is not zero, so there will be no infinite loop
    while (currentStateDurationSampleNum == 0) { // if the next state is consist of 0 sample
      changeStage(realTimePularDutyCycleSamples, realTimePularSilenceSamples, snapShot.interTrainSilenceSamples, snapShot.trainDutyCycleSamples);
    }

    // reset to an idle
    pulsarAdsr.reset();
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * snapShot.pulsarPeriodTime);
    // pulsarAdsr.noteOff();
    // Apply envelope to both pulse and silence to facilitate subsequent masking
    // processing (for example, if the silence of pulsar is converted to pulse
    // and envelope is not enabled here, it will be 0, then it will be 0
    // directly after conversion).
    pulsarAdsr.noteOn();
    isTriggeredReleaseFlag = false;
  }

  bool passMaskFlag = true; // the mask of current sample is 1
  bool existMask = false;
  mask(passMaskFlag, existMask);

  // It used to be pulsar silence and now it has become pulse. To confirm the final frequency of pulsar and the adsr ratio value
  bool silenceToPulseFlag = currentState == why::PulsarStateEnum::IntraSilence && existMask && passMaskFlag;

  // The frequency after pulse duty cycle modulation will affect the length of the pulse
  float newPulsarFreq;
  calcNewPulsarFreq(commonVoiceSate->pulsarDutyCycleRatioParam->load(), newPulsarFreq, silenceToPulseFlag);

  // Modify the adsr time: Note that after masking, silence maybe also be pulse.
  if (silenceToPulseFlag) { // 如果mask后，当前silence也变成了pulse，也重新应用adsr
    refreshPulsaretAdsr((1 - commonVoiceSate->pulsarDutyCycleRatioParam->load()) * snapShot.pulsarPeriodTime);
  } else if (commonVoiceSate->pulsarDutyCycleClusterLenParam->load() > 1) { // pulsarset后，dutycyle也变了，adsr也需要改变
    refreshPulsaretAdsr((commonVoiceSate->pulsarDutyCycleRatioParam->load()) * snapShot.pulsarPeriodTime / commonVoiceSate->pulsarDutyCycleClusterLenParam->load());
  }

  // Trigger the release phase: Within the application duration, there should still be room to apply the release; otherwise, it will not be triggered
  if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && currentStateDurationSampleNum - hasPassedSampleNumInsideTrain <= pulsarAdsrParams.release * getSampleRate()) {
    pulsarAdsr.noteOff();
    isTriggeredReleaseFlag = true;
  }

  // calc sample
  float s = calSampleByState(passMaskFlag, existMask, newPulsarFreq);

  hasPassedSampleNumInsideTrain++;

  // DBG(juce::String::formatted(
  //     "hasPassedSampleNumInsideTrain:%d ; totalSampleCount:%d ; sample:%.5f",
  //     (int)hasPassedSampleNumInsideTrain,
  //     (int)sinceLastTransitionSamplesTotal,
  //     s));
  return s;
}

float PulsarSynthVoice::calcActualPulse(float pulsarModFreq) {
  // 应用 FM 调制（基于 phase 从包络或 LFO 采样）
  float fmModulation = calcFormantLfoInterpolation(pulsaretPhase, pulsarModFreq, 1.0f);
  float modulatedFreq = pulsarModFreq * (1.0f + fmModulation);

  float s = PulsaretWaveformSingleton::getInstance()                                                 //
                .calcSample(commonVoiceSate->pulsarWaveformParam->load(), pulsaretPhase)             // single sample
            * calcAmpLfoInterpolation(pulsaretPhase, 1.0f)                                           // AM - 使用 phase 从包络采样
            * (0.73f + std::sqrt(1.0f - commonVoiceSate->pulsarDutyCycleRatioParam->load()) * 0.27f) //  1 - 0.73
            * 1.0f / std::pow(commonVoiceSate->pulsarDutyCycleClusterLenParam->load(), 0.3f)         // 越大越小 1 - 0.3
            * pulsarAdsr.getNextSample();                                                            // adsr
  // It must be placed after calculating the sample, not before; otherwise, the phase at the starting point will shift
  pulsaretPhase += modulatedFreq / getSampleRate();
  // pulsaretPhase = std::fmod(pulsaretPhase, 1.0f); // subtract is faster fmod
  if (pulsaretPhase > 1.0) {
    pulsaretPhase = pulsaretPhase - 1.0;
  }
  return s;
}

float PulsarSynthVoice::calcFormantLfoInterpolation(float phase, float pulsarModFreq, float amount) {
  float depth = commonVoiceSate->formantFreqLfoDepthParam->load();
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
    // 将频率比转换为调制深度 (-1 到 1 范围，0 表示无调制)
    float modValue = (freqRatio - 1.0f) * depth;
    return modValue * amount;
  }

  // 否则使用 LFO 波形调制
  lfoModulator.setFrequency(pulsarModFreq);
  return lfoModulator.calcSampleAfterFM(depth) * amount;
}

float PulsarSynthVoice::calcAmpLfoInterpolation(float phase, float amount) {
  // 如果 depth 为 0，不应用 AM
  float depth = commonVoiceSate->ampLfoDepthParam->load();
  if (depth <= 0.0f) {
    return amount;
  }

  // 如果启用了包络，从包络数据采样
  if (commonVoiceSate->useAmpEnvelope.load()) {
    // phase: 0.0 - 1.0，映射到 2048 个点
    float envelopeValue = getAmpEnvelopeValueAtPhase(phase);
    // envelopeValue 范围是 yMin - yMax，需要归一化到合适的调制范围
    float yMin = commonVoiceSate->ampEnvelopeYMin.load();
    float yMax = commonVoiceSate->ampEnvelopeYMax.load();
    // 将 envelopeValue 转换为调制系数 (0.0 - 1.0 范围)
    float modFactor = juce::jmap(envelopeValue, yMin, yMax, 0.0f, 1.0f);
    return amount * (1.0f - depth + depth * modFactor);
  }

  // 否则使用原来的 LFO 方式
  lfoModulator.setFrequency(phase); // phase 在这里被当作频率使用（旧代码兼容）
  return lfoModulator.calcSampleAfterAM(depth) * amount;
}

float PulsarSynthVoice::getAmpEnvelopeValueAtPhase(float phase) const {
  const auto &envelopeData = commonVoiceSate->ampEnvelopeData;
  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;

  // phase 归一化到 0-1
  phase = std::fmod(std::abs(phase), 1.0f);

  // 线性插值查找
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;

  float val1 = envelopeData[std::min(index, size - 1)];
  float val2 = envelopeData[std::min(index + 1, size - 1)];

  return val1 + frac * (val2 - val1);
}

float PulsarSynthVoice::getFmEnvelopeValueAtPhase(float phase) const {
  const auto &envelopeData = commonVoiceSate->fmEnvelopeData;
  constexpr int size = EnvelopeCanvas::ENVELOPE_SIZE;

  // phase 归一化到 0-1
  phase = std::fmod(std::abs(phase), 1.0f);

  // 线性插值查找
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;

  float val1 = envelopeData[std::min(index, size - 1)];
  float val2 = envelopeData[std::min(index + 1, size - 1)];

  return val1 + frac * (val2 - val1);
}

float PulsarSynthVoice::getOutputGain() {
  // db to gain
  return commonVoiceSate->outputGainParam == nullptr ? 1 : std::pow(10.0f, commonVoiceSate->outputGainParam->load() / 20.0f);
}

bool PulsarSynthVoice::updateBpmDirectly(float bpm) {
  if (why::bpm.load() != bpm) {
    why::bpm.store(bpm);
    return true;
  }
  return false;
}
