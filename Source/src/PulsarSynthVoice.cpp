//
// Created by Mr. Wang on 2025/4/20.
//
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include "PulsarSynthVoice.h"
#include "PulsaretWaveformSingleton.h"
#include <LfoWaveformSingleton.h>
#include <CommonVoiceSate.h>
#include <__ranges/common_view.h>

#include "PulsarSynthSound.h"
#include "ConvolutionResource.h"

//================================================重写方法================================================
void PulsarSynthVoice::startNote(int midiNoteNumber,
                                 float velocity,
                                 SynthesiserSound* sound,
                                 int currentPitchWheelPosition)
{
    float ratio = midiNoteNumber - 60;
    currentVelocity = velocity; // 保存下来
    //公用一套fundamental frequency，不同voice可以修改其他参数
    //设置train为初始状态
    resetTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()) * (ratio + 1),
               static_cast<int>(commonVoiceSate->trainSilenceParam->load()) * (ratio + 1), true,
               static_cast<int>(commonVoiceSate->trainLenParam->load()));
    juce::ADSR::Parameters param(0.11, 0.0, 1, 0.5);
    envelope.setParameters(param);
    envelope.noteOn();
    playing = true;
}

//allowTailOff true，不会立即停止声音，走envelope noteoff停止；在此期间，renderNextBlock() 仍会继续跑，虽然 envelope 音量很小，但还是在输出 sample。
void PulsarSynthVoice::stopNote(float, bool allowTailOff)
{
    envelope.noteOff();
    clearCurrentNote();
    playing = false; // 防止 render 再进来
}

void PulsarSynthVoice::pitchWheelMoved(int)
{
}

void PulsarSynthVoice::controllerMoved(int, int)
{
}

bool PulsarSynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PulsarSynthSound*>(sound) != nullptr;
}

void PulsarSynthVoice::renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples)
{
    if (!playing)
    {
        return;
    }

    processSampleWithConvolution(outputBuffer, startSample, numSamples, why::PlayModeEnum::Midi);

    if (!envelope.isActive())
    {
        playing = false;
        clearCurrentNote();
    }
}

// ================================================自定义方法================================================
//In auto mode, bypass startnote and directly call to play the sample
void PulsarSynthVoice::renderNextBlockDirectly(juce::AudioSampleBuffer& outputBuffer,
                                               juce::AudioPlayHead* audioPlayHead, int startSample, int numSamples,
                                               why::PlayModeEnum currentPlayModeEnum)
{
    //Control the playback and stop of the DAW to trigger the start and stop of the pulsar process respectively
    if (audioPlayHead != nullptr && audioPlayHead->getPosition())
    {
        bool isNowPlaying = audioPlayHead->getPosition()->getIsPlaying();

        if (isNowPlaying && !wasPlayingLastFrame)
        {
            // Transport just started
            isActive = true;
            soundOffWhenSwitchPlayMode = false;

            resetTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
                       commonVoiceSate->trainSilenceParam->load(), true,
                       commonVoiceSate->trainLenParam->load());
        }

        if (!isNowPlaying && wasPlayingLastFrame)
        {
            // Transport just stopped
            isActive = false;
        }

        wasPlayingLastFrame = isNowPlaying;
    }

    if (isActive && !soundOffWhenSwitchPlayMode)
    {
        processSampleWithConvolution(outputBuffer, startSample, numSamples, currentPlayModeEnum);
    }
}

void PulsarSynthVoice::processSampleWithConvolution(juce::AudioSampleBuffer& outputBuffer, int startSample,
                                                    int numSamples, why::PlayModeEnum currentPlayModeEnum)
{
    commonVoiceSate->pulseBuffer.clear();
    if (commonVoiceSate->pulseBuffer.getNumSamples() < numSamples || commonVoiceSate->pulseBuffer.getNumChannels() <
        outputBuffer.
        getNumChannels())
    {
        commonVoiceSate->pulseBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, true, true); // 自动释放并重新分配
    }
    auto* leftWritePtr = commonVoiceSate->pulseBuffer.getWritePointer(0);
    auto* rightWritePtr = commonVoiceSate->pulseBuffer.getWritePointer(1);

    for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++)
    {
        int i = sampleIndex - startSample;

        float pulse = this->processSample();
        //TODO 相对化音量
        // pulse = (!this->isCurrentCacluatedPulse()) ? pulse * 0.0 : pulse;
        float currentSample = pulse;

        //auto mode outputs the original value, and midi mode applies envelope for each key press
        if (currentPlayModeEnum == why::PlayModeEnum::Midi)
        {
            currentSample = currentSample * envelope.getNextSample() * currentVelocity;
        }

        leftWritePtr[i] = currentSample;
        rightWritePtr[i] = currentSample;
    }

    //process convolution
    if (static_cast<int>(commonVoiceSate->impulseSwitchParam->load()) != static_cast<int>(why::ImpulseSwitchEnum::Off))
    {
        commonVoiceSate->convolutionResource->processSample(commonVoiceSate->pulseBuffer);
    }

    //adjust gain
    // float peak = commonVoiceSate->pulseBuffer.getMagnitude(0, commonVoiceSate->pulseBuffer.getNumSamples());
    // //扩大音量
    // float targetPeak = 0.5f;
    // float gain = targetPeak / (peak + 1e-5f);
    // commonVoiceSate->pulseBuffer.applyGain(gain);
    // // 限制音量
    // float peak = commonVoiceSate->pulseBuffer.getMagnitude(0, commonVoiceSate->pulseBuffer.getNumSamples());
    //
    // float ceiling = 1.0f;
    // if (peak > ceiling)
    // {
    //     float gain = ceiling / (peak + 1e-5f);
    //     commonVoiceSate->pulseBuffer.applyGain(gain);
    // }

    for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++)
    {
        int i = sampleIndex - startSample;
        for (int chan = 0; chan < outputBuffer.getNumChannels(); chan++)
        {
            // The output sample is scaled by 0.2 so that it is not too loud by default
            outputBuffer.addSample(chan, sampleIndex,
                                   commonVoiceSate->pulseBuffer.getSample(0, i) * getOutputGain());
        }
    }
}


void PulsarSynthVoice::setPulsarSilence(float dutyCylceRatio)
{
    pulsarDutyCycleRatio = dutyCylceRatio;
    pulsarSilenceTime = pulsarPeriodTime * (1 - commonVoiceSate->pulsarDutyCycleRatioParam->load());
}

void PulsarSynthVoice::resetTrainInitialSate()
{
    currentState = why::PulsarStateEnum::IntraSilence;
    hasPassedSampleNumInsideTrain = -999;
    trainCounter = 0.0;
    currentStateDurationSampleNum = 0;
    trainPositionSamples = 0;
    pulsarStageIndexInTrainDutyCycle = 0;
    currentSampleInPulse = false;
}

//等待最近的pulsar silence或train interval silence结束再reset并进入新的train
void PulsarSynthVoice::changeToNewTrainAfterPulsarPeriodOrTrainEnd(bool bpmChangedFlag)
{
    int trainIntervalSilenceLen = static_cast<int>(commonVoiceSate->trainSilenceParam->load());
    int trainDutyCycleLen = static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load());
    int trainLen = static_cast<int>(commonVoiceSate->trainLenParam->load());
    int oldTrainIntervalSilenceLen = this->previousTrainSilenceNum;
    int oldTrainDutyCycleLen = this->previousTrainLen;
    int oldTrainLen = this->previousTrainDutyCycleNum;

    if (trainIntervalSilenceLen + trainDutyCycleLen <= 0)
    {
        return;
    }

    // Change the train trajectory: Reset the current position of the train,
    // reset it to the new train and then change it to false (updated in processSample)
    isTheSameTrainConfig = oldTrainIntervalSilenceLen == trainIntervalSilenceLen && oldTrainDutyCycleLen ==
        trainDutyCycleLen && oldTrainLen == trainLen && !bpmChangedFlag;
    if (isTheSameTrainConfig)
    {
        return;
    }

    this->previousTrainSilenceNum = trainIntervalSilenceLen;
    this->previousTrainLen = trainLen;
    this->previousTrainDutyCycleNum = trainDutyCycleLen;

    changeTrainTrace = true;
    //new train config
    newTrainIntervalSilenceLen = trainIntervalSilenceLen;
    newTrainDurationLen = trainDutyCycleLen;
    newTrainLen = trainLen;
}

//Will not wait for build a new train directly, and changeToNewTrainAfterPulsarPeriodOrTrainEnd can wait
void PulsarSynthVoice::resetTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen)
{
    //Rebuild the train config
    initTrain(durationLen, intervalSilenceLen, isLoop, trainLen);
    //Restore the initial state of train
    //pulsar行走过程中用来判断位置的相关samples，即长度即位置
    resetTrainRelatedSamples4Location();
    //train initializes the state
    resetTrainInitialSate();
}

//stochastic mask will not be generated. It will only be generated when the train dutycycle is changed
void PulsarSynthVoice::initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen)
{
    if (intervalSilenceLen + durationLen <= 0)
    {
        return;
    }
    newTrainIntervalSilenceLen = intervalSilenceLen;
    newTrainDurationLen = durationLen;
    newTrainLen = trainLen;

    realChangeTrainConfig();
}

void PulsarSynthVoice::realChangeTrainConfig()
{
    //Currently, it is 1/4 beat. train length is how many 1/4 (1 beat) there are
    trainLenBlock = (60.0 / why::bpm.load());
    trainTime = newTrainLen * trainLenBlock;
    pulsarPeriodTime = trainTime / (newTrainIntervalSilenceLen + newTrainDurationLen);
    setPulsarSilence(commonVoiceSate->pulsarDutyCycleRatioParam->load());
    fundamentalFreq = 1.0 / pulsarPeriodTime;
    trainSilenceTime = newTrainIntervalSilenceLen * pulsarPeriodTime;
    trainDutyCycleTime = newTrainDurationLen * pulsarPeriodTime;

    //train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
    trainPeriodTime = trainDutyCycleTime + trainSilenceTime;
    //If the fundamental freq changes, it must be returned to phase; otherwise, the waveform will shift
    pulsaretPhase = 0.0;

    //更新pulsaret adsr
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::initSynthVoice(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                                      juce::AudioPlayHead* audioPlayHead)
{
    // DBG("HOW MANY TIMES" <<why::get_thread_id_str());
    // initBpmFromDaw(audioPlayHead);

    // why::sampleRate.store(sampleRate);
    commonVoiceSate->sampleBuffer = std::move(sampleBuffer);

    initTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
              commonVoiceSate->trainSilenceParam->load(), true,
              commonVoiceSate->trainLenParam->load());

    commonVoiceSate->generateStochasticMask();

    //刷新adsr
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::connectParameters(juce::AudioProcessorValueTreeState& apvts)
{
    mappingParams(apvts);
}

void PulsarSynthVoice::mappingParams(const juce::AudioProcessorValueTreeState& apvts)
{
    //output
    commonVoiceSate->outputGainParam = apvts.getRawParameterValue(why::ParameterID::outputGain);

    //play mode
    commonVoiceSate->playModeParam = apvts.getRawParameterValue(why::ParameterID::playMode);

    //train
    commonVoiceSate->trainLenParam = apvts.getRawParameterValue(why::ParameterID::trainLen);
    commonVoiceSate->trainDutyCycleLenParam = apvts.getRawParameterValue(why::ParameterID::trainDutyCycleLen);
    commonVoiceSate->trainSilenceParam = apvts.getRawParameterValue(why::ParameterID::trainSilenceLen);

    this->previousTrainSilenceNum = commonVoiceSate->trainSilenceParam->load();
    this->previousTrainLen = commonVoiceSate->trainLenParam->load();
    this->previousTrainDutyCycleNum = commonVoiceSate->trainDutyCycleLenParam->load();

    //pulsar basic info
    commonVoiceSate->pulsarWaveformParam = apvts.getRawParameterValue(why::ParameterID::pulsarWaveform);
    commonVoiceSate->pulsarDutyCycleClusterLenParam = apvts.getRawParameterValue(
        why::ParameterID::pulsarDutyCycleClusterLen);
    commonVoiceSate->pulsarDutyCycleRatioParam = apvts.getRawParameterValue(why::ParameterID::pulsarDutyCycleRatio);

    //pulsar modulation
    commonVoiceSate->formantFreqLfoParam = apvts.getRawParameterValue(why::ParameterID::formantFreqLfoWaveform);
    commonVoiceSate->ampLfoParam = apvts.getRawParameterValue(why::ParameterID::ampLfoWaveform);

    //pulsar envelope
    commonVoiceSate->attackParam = apvts.getRawParameterValue(why::ParameterID::pulsarAttack);
    commonVoiceSate->decayParam = apvts.getRawParameterValue(why::ParameterID::pulsarDecay);
    commonVoiceSate->sustainParam = apvts.getRawParameterValue(why::ParameterID::pulsarSustain);
    commonVoiceSate->releaseParam = apvts.getRawParameterValue(why::ParameterID::pulsarRelease);

    //masking
    commonVoiceSate->maskOptionParam = apvts.getRawParameterValue(why::ParameterID::maskOption);
    commonVoiceSate->maskOption = static_cast<why::MaskOptionEnum>(static_cast<int>(commonVoiceSate->maskOptionParam->
        load()));

    //burst mask
    //texteditor must be passed through property and is not directly bound by attachment
    if (!apvts.state.getProperty(why::PropertyID::burstMask).isVoid())
    {
        //get texteditor value from property because juce can't bind texteditor with parameter automatally
        commonVoiceSate->burstMask = apvts.state.getProperty(why::PropertyID::burstMask).toString().toStdString();
    }

    //stochastic mask
    if (!apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid())
    {
        commonVoiceSate->stochasticMaskStr = apvts.state.getProperty(why::PropertyID::stochasticMask).toString().
                                                   toStdString();
    }

    //euclid mask
    commonVoiceSate->euclidStepsParam = apvts.getRawParameterValue(why::ParameterID::euclidSteps);
    commonVoiceSate->euclidHitsParam = apvts.getRawParameterValue(why::ParameterID::euclidHits);

    if (commonVoiceSate->euclidStepsParam->load() > 0 && commonVoiceSate->euclidHitsParam->load() > 0)
    {
        //generate euclid rhythm pattern
        commonVoiceSate->euclids = why::generateEuclidRhythm(commonVoiceSate->euclidStepsParam->load(),
                                                             commonVoiceSate->euclidHitsParam->load());
    }

    //impulse switch
    commonVoiceSate->impulseSwitchParam = apvts.getRawParameterValue(why::ParameterID::impulseSwitch);
}


void PulsarSynthVoice::mappingOneParam(const juce::AudioProcessorValueTreeState& apvts, juce::String parameterID,
                                       float newValue)
{
    //output
    if (parameterID == why::ParameterID::outputGain)
    {
        commonVoiceSate->outputGainParam->store(newValue);
    }

    //play mode
    if (parameterID == why::ParameterID::playMode)
    {
        commonVoiceSate->playModeParam->store(newValue);
    }

    //train
    if (parameterID == why::ParameterID::trainLen)
    {
        commonVoiceSate->trainLenParam->store(newValue);
    }
    if (parameterID == why::ParameterID::trainDutyCycleLen)
    {
        commonVoiceSate->trainDutyCycleLenParam->store(newValue);
    }
    if (parameterID == why::ParameterID::trainSilenceLen)
    {
        commonVoiceSate->trainSilenceParam->store(newValue);
    }

    //pulsar basic info
    if (parameterID == why::ParameterID::pulsarWaveform)
    {
        commonVoiceSate->pulsarWaveformParam->store(newValue);
    }
    if (parameterID == why::ParameterID::pulsarDutyCycleClusterLen)
    {
        commonVoiceSate->pulsarDutyCycleClusterLenParam->store(newValue);
    }
    if (parameterID == why::ParameterID::pulsarDutyCycleRatio)
    {
        commonVoiceSate->pulsarDutyCycleRatioParam->store(newValue);
    }

    //pulsar modulation
    if (parameterID == why::ParameterID::formantFreqLfoWaveform)
    {
        commonVoiceSate->formantFreqLfoParam->store(newValue);
    }
    if (parameterID == why::ParameterID::ampLfoWaveform)
    {
        commonVoiceSate->ampLfoParam->store(newValue);
    }

    //pulsar envelope
    if (parameterID == why::ParameterID::pulsarAttack)
    {
        commonVoiceSate->attackParam->store(newValue);
    }
    if (parameterID == why::ParameterID::pulsarDecay)
    {
        commonVoiceSate->decayParam->store(newValue);
    }
    if (parameterID == why::ParameterID::pulsarSustain)
    {
        commonVoiceSate->sustainParam->store(newValue);
    }
    if (parameterID == why::ParameterID::pulsarRelease)
    {
        commonVoiceSate->releaseParam->store(newValue);
    }

    //masking
    if (parameterID == why::ParameterID::maskOption)
    {
        commonVoiceSate->maskOptionParam->store(newValue);
        commonVoiceSate->maskOption = static_cast<why::MaskOptionEnum>(static_cast<int>(commonVoiceSate->maskOptionParam
            ->load()));
    }

    //burst mask
    //texteditor must be passed through property and is not directly bound by attachment
    if (!apvts.state.getProperty(why::PropertyID::burstMask).isVoid())
    {
        commonVoiceSate->burstMask = apvts.state.getProperty(why::PropertyID::burstMask).toString().toStdString();
    }

    //stochastic mask
    if (!apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid())
    {
        commonVoiceSate->stochasticMaskStr = apvts.state.getProperty(why::PropertyID::stochasticMask).toString().
                                                   toStdString();
    }

    //euclid mask
    if (parameterID == why::ParameterID::euclidSteps)
    {
        commonVoiceSate->euclidStepsParam->store(newValue);
    }
    if (parameterID == why::ParameterID::euclidHits)
    {
        commonVoiceSate->euclidHitsParam->store(newValue);
    }

    if (commonVoiceSate->euclidStepsParam->load() > 0 && commonVoiceSate->euclidHitsParam->load() > 0)
    {
        //generate euclid rhythm pattern
        commonVoiceSate->euclids = why::generateEuclidRhythm(commonVoiceSate->euclidStepsParam->load(),
                                                             commonVoiceSate->euclidHitsParam->load());
    }

    //impulse switch
    if (parameterID == why::ParameterID::impulseSwitch)
    {
        commonVoiceSate->impulseSwitchParam->store(newValue);
    }
}

void PulsarSynthVoice::parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID,
                                        float newValue, bool& isGeneratedStochasticMask)
{
    mappingOneParam(apvts, parameterID, newValue);
    bool updatedBpm = false;

    if (parameterID == why::ParameterID::bpm)
    {
        updatedBpm = updateBpmDirectly(newValue);
    }

    //When the parameters related to train change, the pulsar frequency will also change. At this time,
    //wait for the next pulsar silence or train interval silence to start a new train
    if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen ||
        parameterID == why::ParameterID::trainLen || parameterID == why::ParameterID::bpm)
    {
        changeToNewTrainAfterPulsarPeriodOrTrainEnd(updatedBpm);

        if (parameterID == why::ParameterID::trainDutyCycleLen)
        {
            //根据train长度生成对应随机mask
            commonVoiceSate->generateStochasticMask();
            isGeneratedStochasticMask = true;
        }
    }

    setPulsarSilence(commonVoiceSate->pulsarDutyCycleRatioParam->load());
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::reloadPreset(juce::AudioProcessorValueTreeState& apvts)
{
    mappingParams(apvts);

    initTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
              commonVoiceSate->trainSilenceParam->load(), true,
              commonVoiceSate->trainLenParam->load());

    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::refreshPulsaretAdsr(float pulsarDutyCycleTime)
{
    pulsarAdsr.setSampleRate(getSampleRate());
    float attack = commonVoiceSate->attackParam->load();
    float decay = commonVoiceSate->decayParam->load();
    float release = commonVoiceSate->releaseParam->load();

    //Control ratio(a+d+s+r) <= 1.0 cut the redundant part
    if (attack + decay + release > 1 && attack + decay >= 1)
    {
        release = 0;
        if (attack >= 1)
        {
            decay = 0;
        }
        if (attack < 1)
        {
            decay = 1 - attack;
        }
    }
    if (attack + decay + release > 1 && attack + decay < 1)
    {
        release = 1 - attack - decay;
    }

    pulsarAdsrParams.attack = pulsarDutyCycleTime * attack;
    pulsarAdsrParams.decay = pulsarDutyCycleTime * decay;
    pulsarAdsrParams.sustain = commonVoiceSate->sustainParam->load();
    pulsarAdsrParams.release = pulsarDutyCycleTime * release;

    pulsarAdsr.setParameters(pulsarAdsrParams);
}

void PulsarSynthVoice::mask(bool& maskPassFlag, bool& existMask)
{
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::Off)
    {
        existMask = false;
        //Do not perform mask processing and return according to the original pulsaret
        maskPassFlag = true;
        return;
    }
    //burst masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::BurstMask && !commonVoiceSate->burstMask.empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        existMask = true;
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->burstMask.size());
        maskPassFlag = commonVoiceSate->burstMask[index] == '1';
    }
    //euclid masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::EuclidMask && !commonVoiceSate->euclids.empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        existMask = true;
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->euclids.size());
        maskPassFlag = commonVoiceSate->euclids[index] == '1';
    }
    //stochastic masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::StochasticMask && !commonVoiceSate->stochasticMaskStr.
        empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        existMask = true;
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->stochasticMaskStr.size());
        maskPassFlag = commonVoiceSate->stochasticMaskStr[index] == '1';
    }
}

void PulsarSynthVoice::changeStage(int pulsarDutyCycleSamples, int intraSilenceSamples, int interTrainSilenceSamples,
                                   int trainDutyCycleSamples)
{
    hasPassedSampleNumInsideTrain = 0;
    //Note that the silence of the mask may also become a pulse. A pulsar cluster can contain multiple pulsaret,
    //and this is the final smallest phase
    pulsaretPhase = 0.0f;

    switch (currentState)
    {
    case why::PulsarStateEnum::Pulse: //The duty cycle stage of pulsar has been completed at present
        //Check if there is still time to continue this train
        if (trainPositionSamples + intraSilenceSamples <= trainDutyCycleSamples)
        {
            currentState = why::PulsarStateEnum::IntraSilence;
            currentStateDurationSampleNum = intraSilenceSamples;
            //The total duration corresponding to the new state
            trainPositionSamples += intraSilenceSamples;

            pulsarStageIndexInTrainDutyCycle++;
        }
        else
        {
            // train is over. Enter the train room and mute
            currentState = why::PulsarStateEnum::InterTrainSilence;
            currentStateDurationSampleNum = interTrainSilenceSamples;
            trainPositionSamples = 0;
            pulsarStageIndexInTrainDutyCycle = 0;
        }
        break;
    case why::PulsarStateEnum::IntraSilence: //The silence stage of pulsar has now been completed
        if ((trainPositionSamples + pulsarDutyCycleSamples) <= trainDutyCycleSamples)
        {
            currentState = why::PulsarStateEnum::Pulse;
            currentStateDurationSampleNum = pulsarDutyCycleSamples;
            trainPositionSamples += pulsarDutyCycleSamples;
            pulsarStageIndexInTrainDutyCycle++;
        }
        else
        {
            // If the remaining time is insufficient to complete the full cycle, simply end the train
            currentState = why::PulsarStateEnum::InterTrainSilence;
            currentStateDurationSampleNum = interTrainSilenceSamples;
            trainPositionSamples = 0;
            pulsarStageIndexInTrainDutyCycle = 0;
        }
        break;
    case why::PulsarStateEnum::InterTrainSilence: //train silence finished
        trainCounter++;
    //Restore to the initial state of train
        currentState = why::PulsarStateEnum::IntraSilence;
        currentStateDurationSampleNum = 0.0f;
        trainPositionSamples = 0;
        pulsarStageIndexInTrainDutyCycle = 0;
        break;
    }
}

//modulation will affect the length of the pulse
void PulsarSynthVoice::calcNewPulsarFreq(float& pulsarModFreq, bool silenceToPulseFlag)
{
    //The frequency of the pulse is finally by duty cycle
    pulsarModFreq =
        // commonVoiceSate->pulsarDutyCycleClusterLenParam->load() *
        1.0 / (commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
    //当ratio为1时，实际上也不存在silence了，所以无需计算新的freq
    if (silenceToPulseFlag && 1 != commonVoiceSate->pulsarDutyCycleRatioParam->load())
    {
        pulsarModFreq =
            // commonVoiceSate->pulsarDutyCycleClusterLenParam->load() *
            1.0 / ((1 - commonVoiceSate->pulsarDutyCycleRatioParam->load()) * pulsarPeriodTime);
    }
    float modFreq = calcFormantLfoInterpolation(pulsarModFreq, commonVoiceSate->sustainParam->load());
    pulsarModFreq = pulsarModFreq + pulsarModFreq * modFreq;
}

float PulsarSynthVoice::calSampleByState(bool passMaskFlag, bool existMask, float pulsarModFreq)
{
    switch (currentState)
    {
    case why::PulsarStateEnum::Pulse:
    // If a mask exists, sounds may be emitted during the silence stage; otherwise, all are zeros
    case why::PulsarStateEnum::IntraSilence:
        if (!existMask && currentState == why::PulsarStateEnum::IntraSilence)
        {
            currentSampleInPulse = false;
            return 0;
        }
    //mask status approve passing
        if (!passMaskFlag)
        {
            currentSampleInPulse = false;
            return 0;
        }
        currentSampleInPulse = true;
        return calcActualPulse(pulsarModFreq);
    case why::PulsarStateEnum::InterTrainSilence:
        currentSampleInPulse = false;
        return 0.0f;
    }
    return 0.0f;
}

void PulsarSynthVoice::resetTrainRelatedSamples4Location()
{
    pulsarDutyCycleSamples = commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime * getSampleRate();
    pulsarIntraSilenceSamples = (1 - commonVoiceSate->pulsarDutyCycleRatioParam->load()) * pulsarPeriodTime *
        getSampleRate();
    interTrainSilenceSamples = trainSilenceTime * getSampleRate();
    trainDutyCycleSamples = trainDutyCycleTime * getSampleRate();
}

float PulsarSynthVoice::processSample()
{
    //once playback, default is loop and can not be updated
    if (!commonVoiceSate->isLoop && trainCounter >= 1)
    {
        return 0;
    }

    //train长度为0不处理
    if (trainDutyCycleTime <= 0)
    {
        return 0.0;
    }

    resetTrainRelatedSamples4Location();

    // State switching: Each time the current state is reached, the next stage will begin
    if (hasPassedSampleNumInsideTrain == -999 || hasPassedSampleNumInsideTrain == currentStateDurationSampleNum)
    {
        //Enter the latest train: Enter after pulsar silence or train silence ends.
        //If both silence are 0, directly enter the new train after pulse
        if (changeTrainTrace && (currentState == why::PulsarStateEnum::IntraSilence || currentState ==
            why::PulsarStateEnum::InterTrainSilence || (commonVoiceSate->pulsarDutyCycleRatioParam->load() == 1 &&
                commonVoiceSate->trainSilenceParam->load() == 0)))
        {
            //Initialize according to the new config
            resetTrain(newTrainDurationLen, newTrainIntervalSilenceLen, true, newTrainLen);
            changeTrainTrace = false;
        }

        changeStage(pulsarDutyCycleSamples, pulsarIntraSilenceSamples, interTrainSilenceSamples, trainDutyCycleSamples);

        //If the length of a certain state is 0 samples, it means that the process of the current process should (must)
        //automatically jump to the next stage. Because as the stages change, there will definitely be parts
        //where the stage length is not zero, so there will be no infinite loop
        while (currentStateDurationSampleNum == 0)
        {
            //继续change stage，以免无需存在的部分，占据sample，尽管是返回0值的sample
            changeStage(pulsarDutyCycleSamples, pulsarIntraSilenceSamples, interTrainSilenceSamples,
                        trainDutyCycleSamples);
        }

        pulsarAdsr.reset();
        // Apply envelope to both pulse and silence to facilitate subsequent masking processing
        // (for example, if the silence of pulsar is converted to pulse and envelope is not enabled here,
        // it will be 0, then it will be 0 directly after conversion).
        pulsarAdsr.noteOn();
        isTriggeredReleaseFlag = false;
    }

    bool passMaskFlag = true;
    bool existMask = false;
    mask(passMaskFlag, existMask);

    //It used to be pulsar silence and now it has become pulse. To confirm
    //the final frequency of pulsar and the adsr ratio value
    bool silenceToPulseFlag = existMask && currentState == why::PulsarStateEnum::IntraSilence && passMaskFlag;

    //The frequency after pulse duty cycle modulation will affect the length of the pulse
    float pulsarModFreq;
    calcNewPulsarFreq(pulsarModFreq, silenceToPulseFlag);

    // Modify the adsr time: Note that after masking, silence may also be pulse.
    // The proportional reference value of adsr should be adjusted in real time, that is,
    // the duration of pulsar dutycycle or pulsar silence
    if (silenceToPulseFlag)
    {
        refreshPulsaretAdsr((1 - this->pulsarDutyCycleRatio) * this->pulsarPeriodTime);
    }
    else
    {
        refreshPulsaretAdsr((this->pulsarDutyCycleRatio) * this->pulsarPeriodTime);
    }
    //Trigger the release phase: Note that within the application duration,
    //there should still be room to apply the release; otherwise, it will not be triggered
    if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && currentStateDurationSampleNum -
        hasPassedSampleNumInsideTrain <= pulsarAdsrParams.release * getSampleRate())
    {
        pulsarAdsr.noteOff();
        isTriggeredReleaseFlag = true;
    }

    //calc sample
    float s = calSampleByState(passMaskFlag, existMask, pulsarModFreq);

    hasPassedSampleNumInsideTrain++;

    // DBG(juce::String::formatted(
    //     "hasPassedSampleNumInsideTrain:%d ; totalSampleCount:%d ; sample:%.5f",
    //     (int)hasPassedSampleNumInsideTrain,
    //     (int)sinceLastTransitionSamplesTotal,
    //     s));
    return s;
}

float PulsarSynthVoice::calcActualPulse(float pulsarModFreq)
{
    float s =
        PulsaretWaveformSingleton::getInstance().calcSample(
            commonVoiceSate->pulsarWaveformParam->load(), pulsaretPhase
        ) * calcAmpLfoInterpolation(pulsarModFreq, commonVoiceSate->sustainParam->load()) * pulsarAdsr.getNextSample();
    //It must be placed after calculating the sample, not before; otherwise, the phase at the starting point will shift
    pulsaretPhase += pulsarModFreq / getSampleRate();
    // pulsaretPhase = std::fmod(pulsaretPhase, 1.0f); //subtract is faster fmod
    if (pulsaretPhase > 1.0)
    {
        pulsaretPhase = pulsaretPhase - 1.0;
    }
    return s;

    //TODO 扫描sample播放，use sample file as pulsaret waveform
    // float s;
    // if (sampleBuffer != nullptr)
    // {
    //     int index1 = static_cast<int>(readHead);
    //     int index2 = index1 + 1;
    //     float frac = readHead - index1;
    //
    //     if (index1 >= 0 && index2 < sampleBuffer->getNumSamples())
    //     {
    //         float s1 = sampleBuffer->getReadPointer(0)[index1];
    //         float s2 = sampleBuffer->getReadPointer(0)[index2];
    //         s = (1.0f - frac) * s1 + frac * s2; // 线性插值
    //     }
    //     //根据当前pulse duty cycle长度，来判断sample需要以什么速度播放，从而确认以多少step读取samples
    //     //readHea按2个steps读取就是倍速，3个就是3倍速
    //     //pulse如果是2hz，就表示1秒2次，所以速度就2倍速
    //     //在指定时长内播完采样
    //     //调制后的频率得到新的duty cycle长度
    //     readHead += sampleBuffer->getNumSamples() / ((1.0 / pulsarModFreq) * why::sampleRate.load());
    //     readHead = std::fmod(readHead, sampleBuffer->getNumSamples());
    //     //扫描sample播放
    // }
    //==============hann window实现每个pulse两头衰减为0==============
    // 计算衰减的起始和结束点
    // float fadeStart = 0.95; // 衰减开始的位置
    // float fadeEnd = 1.0f - fadeStart; // 衰减结束的位置
    // 计算窗口位置
    // float hann = 1.0f; // 默认值为 1 (没有衰减)
    //
    // // 在衰减范围内计算窗口
    // if (pulsaretPhase < fadeStart)
    // {
    //     // 衰减部分（前 10%）
    //     hann = 0.5f *
    //         (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (pulsaretPhase / fadeStart)));
    // }
    // else if (pulsaretPhase > fadeEnd)
    // {
    //     // 衰减部分（后 10%）
    //     hann = 0.5f * (1.0f - std::cos(
    //         2.0f * juce::MathConstants<float>::pi * ((1.0f - pulsaretPhase) / (1.0f - fadeEnd))));
    // }
    //float hann = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * pulsaretPhase));

    //========================================================
    //return ((s + calcSample()) / 2.0) * calcAmpLfoInterpolation() * adsr.getNextSample() * hann;
    //计算pulsaret waveform sample
}

float PulsarSynthVoice::calcFormantLfoInterpolation(float pulsarModFreq, float amount)
{
    if (commonVoiceSate->formantFreqLfoParam->load() <= 0.0)
    {
        return 0.0;
    }
    LfoWaveformSingleton& lfo = LfoWaveformSingleton::getInstance(getSampleRate(), pulsarModFreq);
    return lfo.calcSampleAfterFM(commonVoiceSate->formantFreqLfoParam->load(), pulsaretPhase) * amount;
}

float PulsarSynthVoice::calcAmpLfoInterpolation(float pulsarModFreq, float amount)
{
    if (commonVoiceSate->ampLfoParam->load() <= 0.0)
    {
        return amount;
    }
    LfoWaveformSingleton& lfo = LfoWaveformSingleton::getInstance(getSampleRate(), pulsarModFreq);
    return lfo.calcSampleAfterAM(commonVoiceSate->ampLfoParam->load(), pulsaretPhase) * amount;
}

float PulsarSynthVoice::getOutputGain()
{
    //db to gain
    return commonVoiceSate->outputGainParam == nullptr
               ? 1
               : std::pow(10.0f, commonVoiceSate->outputGainParam->load() / 20.0f);
}

//TODO ready to discard
int PulsarSynthVoice::initBpmFromDaw(juce::AudioPlayHead* audioPlayHead)
{
    //不在DAW中运行，则为NULL
    if (auto* temp = audioPlayHead)
    {
        if (audioPlayHead->getPosition() && why::bpm.load() != audioPlayHead->getPosition()->getBpm().hasValue())
        {
            why::bpm.store(*audioPlayHead->getPosition()->getBpm());
            return 1;
        }
    }
    return 0;
}

bool PulsarSynthVoice::updateBpmDirectly(float bpm)
{
    if (why::bpm.load() != bpm)
    {
        why::bpm.store(bpm);
        return true;
    }
    return false;
}