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
    // envelope.noteOff();
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
    realTotalSampleCount++;
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
//auto模式下绕过startnote，直接调用，直接播放sample
void PulsarSynthVoice::renderNextBlockDirectly(juce::AudioSampleBuffer& outputBuffer,
                                               juce::AudioPlayHead* audioPlayHead, int startSample, int numSamples,
                                               why::PlayModeEnum currentPlayModeEnum)
{
    //控制DAW播放和停止，分别触发pulsar process的开始和停止
    AudioPlayHead::CurrentPositionInfo pos;
    if (audioPlayHead != nullptr && audioPlayHead->getCurrentPosition(pos))
    {
        bool isNowPlaying = pos.isPlaying;

        if (isNowPlaying && !wasPlayingLastFrame)
        {
            // Transport just started
            isActive = true;
            soundOffWhenSwitchPlayMode = false;
            //设置train为初始状态
            resetTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
                       commonVoiceSate->trainSilenceParam->load(), true,
                       commonVoiceSate->trainLenParam->load());
        }

        if (!isNowPlaying && wasPlayingLastFrame)
        {
            // Transport just stopped
            isActive = false;
            //如果模式刚转换过来，则需要reset train
        }

        wasPlayingLastFrame = isNowPlaying;
    }

    //正在播放
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
    //卷积处理
    //创建一个临时的 AudioBuffer 来存放你生成的 pulse 信号
    for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++)
    {
        int i = sampleIndex - startSample;

        float pulse = this->processSample();
        //相对化音量
        pulse = (!this->isCurrentCacluatedPulse()) ? pulse * 0.0 : pulse;
        float currentSample = pulse;

        //auto输出原始值，midi模式针对每个按键都应用envelope
        if (currentPlayModeEnum == why::PlayModeEnum::Midi)
        {
            //按键包络
            currentSample = currentSample * envelope.getNextSample() * currentVelocity;
        }

        leftWritePtr[i] = currentSample;
        rightWritePtr[i] = currentSample;
    }

    //卷积处理
    if (static_cast<int>(commonVoiceSate->impulseSwitchParam->load()) != static_cast<int>(why::ImpulseSwitchEnum::Off))
    {
        commonVoiceSate->convolutionResource->processSample(commonVoiceSate->pulseBuffer);
    }

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


void PulsarSynthVoice::setPulsarSilence(float dutyCylce)
{
    pulsarDutyCycleRatio = dutyCylce;
    pulsarSilence = pulsarPeriodTime * (1 - commonVoiceSate->pulsarDutyCycleRatioParam->load());
}

void PulsarSynthVoice::resetTrainInitialSate()
{
    currentState = why::PulsarStateEnum::IntraSilence;
    //清空process sample状态
    totalSampleCount = 0;
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
    //在次之前已记录了旧值
    int trainIntervalSilenceLen = static_cast<int>(commonVoiceSate->trainSilenceParam->load());
    int trainDutyCycleLen = static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load());
    int trainLen = static_cast<int>(commonVoiceSate->trainLenParam->load());
    int oldTrainIntervalSilenceLen = this->previousTrainSilenceNum;
    int oldTrainDutyCycleLen = this->previousTrainLen;
    int oldTrainLen = this->previousTrainDutyCycleNum;

    //train长度由BPM确定，则可以计算缩放后的pulsar period
    if (trainIntervalSilenceLen + trainDutyCycleLen <= 0)
    {
        return;
    }
    //改变train轨迹：等到当前PulsarStateEnum阶段结束（会按新频率走pulsaret的sample生成），就进入新的train
    //重置train的当前位置，重置为新的train后再改为false（processSample中更新）
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
    //新的train配置
    newTrainIntervalSilenceLen = trainIntervalSilenceLen;
    newTrainDurationLen = trainDutyCycleLen;
    newTrainLen = trainLen;
}

//不会等待直接build新的train，而changeToNewTrainAfterPulsarPeriodOrTrainEnd会等待
void PulsarSynthVoice::resetTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen)
{
    //重新构建train配置
    initTrain(durationLen, intervalSilenceLen, isLoop, trainLen);
    //恢复train初始的状态
    //pulsar行走过程中用来判断位置的相关samples，即长度即位置
    resetTrainRelatedSamples4Location();
    //train初始化状态
    resetTrainInitialSate();
}

//不会生成stochastic mask，只会在变更train dutycycle时才会生成
void PulsarSynthVoice::initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen)
{
    //train长度由BPM确定，则可以计算缩放后的pulsar period
    if (intervalSilenceLen + durationLen <= 0)
    {
        return;
    }
    //新的train配置
    newTrainIntervalSilenceLen = intervalSilenceLen;
    newTrainDurationLen = durationLen;
    newTrainLen = trainLen;

    realChangeTrain();
}

void PulsarSynthVoice::realChangeTrain()
{
    //目前为1/4拍，train length就是多少个1/4拍
    trainLenBlock = (60.0 / why::bpm.load());
    trainTime = newTrainLen * trainLenBlock;
    pulsarPeriodTime = trainTime / (newTrainIntervalSilenceLen + newTrainDurationLen);
    setPulsarSilence(commonVoiceSate->pulsarDutyCycleRatioParam->load());
    fundamentalFreq = 1.0 / pulsarPeriodTime;
    trainSilenceTime = newTrainIntervalSilenceLen * pulsarPeriodTime;
    trainDutyCycleTime = newTrainDurationLen * pulsarPeriodTime;

    //train period = pulsar time ( = n*(pulsar duty cyle duration + pulsar silence) ) + train interval silence
    trainPeriodTime = trainDutyCycleTime + trainSilenceTime;
    //fundamental freq发生变化，必须归位phase，否则波形偏移了
    pulsaretPhase = 0.0;

    //更新pulsaret adsr
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::initSynth(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                                 juce::AudioPlayHead* audioPlayHead)
{
    // DBG("HOW MANY TIMES" <<why::get_thread_id_str());
    refreshBpm(audioPlayHead);

    // why::sampleRate.store(sampleRate);
    commonVoiceSate->sampleBuffer = std::move(sampleBuffer);

    initTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
              commonVoiceSate->trainSilenceParam->load(), true,
              commonVoiceSate->trainLenParam->load());

    //根据train长度生成对应随机mask
    generateStochasticMask();

    //刷新adsr
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

std::mutex strMutex;
//只会在初始化synth时和parameterChanged中（改变了train dutycycle length）触发
void PulsarSynthVoice::generateStochasticMask()
{
    //midi模式下可能会多个voice来修改stochastic
    std::lock_guard<std::mutex> guard(strMutex);
    //generate random mask, 它的长度是train duty cycle（即pulsar period个数）长度的两倍
    int totalPeriodNum = commonVoiceSate->trainDutyCycleLenParam->load();
    //控制长度，random mask超过64个就限制
    while (totalPeriodNum > 64)
    {
        totalPeriodNum = totalPeriodNum / 2;
    }
    int num = static_cast<int>(2 * totalPeriodNum);
    commonVoiceSate->stochasticMaskStr = why::generateBinaryString(num);
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
    //texteditor必须通过property传递，没有attachment直接绑定
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
    //texteditor必须通过property传递，没有attachment直接绑定
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

//针对单个参数调节
void PulsarSynthVoice::parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID,
                                        float newValue, bool& isGeneratedStochasticMask)
{
    mappingOneParam(apvts, parameterID, newValue);
    bool updatedBpm = false;
    //如果修改了速度
    if (parameterID == why::ParameterID::bpm)
    {
        updatedBpm = updateBpmDirectly(newValue);
    }
    //train相关参数变了，pulsar频率也会变化，此时等待下一个pulsar silence or train interval silence就开始走新的train
    if (parameterID == why::ParameterID::trainDutyCycleLen || parameterID == why::ParameterID::trainSilenceLen ||
        parameterID == why::ParameterID::trainLen || parameterID == why::ParameterID::bpm)
    {
        changeToNewTrainAfterPulsarPeriodOrTrainEnd(updatedBpm);
        //loading preset时，不生成随机mask，只有手动调整UI才会触发
        if (parameterID == why::ParameterID::trainDutyCycleLen)
        {
            //根据train长度生成对应随机mask
            generateStochasticMask();
            isGeneratedStochasticMask = true;
        }
    }

    //刷新pulsar
    setPulsarSilence(commonVoiceSate->pulsarDutyCycleRatioParam->load());
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

//重载一整个preset
void PulsarSynthVoice::reloadPreset(juce::AudioProcessorValueTreeState& apvts)
{
    mappingParams(apvts);

    initTrain(static_cast<int>(commonVoiceSate->trainDutyCycleLenParam->load()),
              commonVoiceSate->trainSilenceParam->load(), true,
              commonVoiceSate->trainLenParam->load());

    //刷新pulsar adsr
    refreshPulsaretAdsr(commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
}

void PulsarSynthVoice::refreshPulsaretAdsr(float pulsarDutyCycleTime)
{
    pulsarAdsr.setSampleRate(getSampleRate());
    float attack = commonVoiceSate->attackParam->load();
    float decay = commonVoiceSate->decayParam->load();
    float release = commonVoiceSate->releaseParam->load();

    //控制比例<=1
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
        //不做mask处理，按原有pulsaret返回
        maskPassFlag = true;
        return;
    }
    //burst masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::BurstMask && !commonVoiceSate->burstMask.empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        existMask = true;
        //如果设置了masking，则重新设置
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->burstMask.size());
        maskPassFlag = commonVoiceSate->burstMask[index] == '1';
    }
    //euclid masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::EuclidMask && !commonVoiceSate->euclids.empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        existMask = true;
        //如果设置了masking，则重新设置
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->euclids.size());
        maskPassFlag = commonVoiceSate->euclids[index] == '1';
    }
    //stochastic masking
    if (commonVoiceSate->maskOption == why::MaskOptionEnum::StochasticMask && !commonVoiceSate->stochasticMaskStr.
        empty() &&
        pulsarStageIndexInTrainDutyCycle > 0)
    {
        //每次修改train lenth，pulsar个数等都修改stochastic mask
        existMask = true;
        int index = fmod(pulsarStageIndexInTrainDutyCycle - 1, commonVoiceSate->stochasticMaskStr.size());
        maskPassFlag = commonVoiceSate->stochasticMaskStr[index] == '1';
    }
}

void PulsarSynthVoice::changeStage(int pulsarSamples, int intraSilenceSamples, int interTrainSilenceSamples,
                                   int trainDutyCycleSamples)
{
    hasPassedSampleNumInsideTrain = 0;
    pulsaretPhase = 0.0f; //针对单个pulse才有phase，注意mask的silence也可能会变成pulse，pulsar cluster可以包含多个pulsaret

    //更新状态，train的位置等信息
    switch (currentState)
    {
    case why::PulsarStateEnum::Pulse: //当前已走完pulsar的duty cycle阶段
        //检查是否还有时间继续这个train
        if (trainPositionSamples + intraSilenceSamples <= trainDutyCycleSamples)
        {
            currentState = why::PulsarStateEnum::IntraSilence;
            currentStateDurationSampleNum = intraSilenceSamples;
            //新状态对应的总历时
            trainPositionSamples += intraSilenceSamples;

            pulsarStageIndexInTrainDutyCycle++;
        }
        else
        {
            // train结束，进入train间静音
            currentState = why::PulsarStateEnum::InterTrainSilence;
            //剩余不再继续进行pulsar period的部分，则直接silence，然后再加上下个周期时长
            currentStateDurationSampleNum = interTrainSilenceSamples;
            trainPositionSamples = 0;
            pulsarStageIndexInTrainDutyCycle = 0;
        }
        break;
    case why::PulsarStateEnum::IntraSilence: //当前已走完pulsar的silence阶段
        // 检查是否还能容纳下一个完整周期
        if ((trainPositionSamples + pulsarSamples) <= trainDutyCycleSamples)
        {
            currentState = why::PulsarStateEnum::Pulse;
            currentStateDurationSampleNum = pulsarSamples;
            trainPositionSamples += pulsarSamples;
            pulsarStageIndexInTrainDutyCycle++;
        }
        else
        {
            // 剩余时间不足完整周期，直接结束train
            currentState = why::PulsarStateEnum::InterTrainSilence;
            //剩余不再继续进行pulsar period的部分，则直接silence，再加上train interval time
            currentStateDurationSampleNum = interTrainSilenceSamples;
            trainPositionSamples = 0;
            pulsarStageIndexInTrainDutyCycle = 0;
        }
        break;
    case why::PulsarStateEnum::InterTrainSilence: //train结束后，在train和train之间的silence阶段
        trainCounter++;
    //恢复到train的初始状态
        currentState = why::PulsarStateEnum::IntraSilence;
        currentStateDurationSampleNum = 0.0f;
        trainPositionSamples = 0;
        pulsarStageIndexInTrainDutyCycle = 0;
        break;
    }
}

//pulse duty cycle调制后的频率大小，将会影响pulse的长度
void PulsarSynthVoice::calcNewPulsarFreq(float& pulsarModFreq, bool silenceToPulseFlag)
{
    //最终决定pulse的频率，因为一个pulsar dutycyle中可以是包含多个pulse的cluster
    pulsarModFreq =
        commonVoiceSate->pulsarDutyCycleClusterLenParam->load() *
        1.0 / (commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime);
    //当ratio为1时，实际上也不存在silence了，所以无需计算新的freq
    if (silenceToPulseFlag && 1 != commonVoiceSate->pulsarDutyCycleRatioParam->load())
    {
        pulsarModFreq =
            commonVoiceSate->pulsarDutyCycleClusterLenParam->load() *
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
    case why::PulsarStateEnum::IntraSilence: //如果存在mask，则silence阶段可能发出声音，否则全是0
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
    pulsarSamples = commonVoiceSate->pulsarDutyCycleRatioParam->load() * pulsarPeriodTime * getSampleRate();
    intraSilenceSamples = (1 - commonVoiceSate->pulsarDutyCycleRatioParam->load()) * pulsarPeriodTime * getSampleRate();
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

    // 状态切换：每次达到当前状态的时间最大值，就会进入下一个阶段
    if (hasPassedSampleNumInsideTrain == -999 || hasPassedSampleNumInsideTrain == currentStateDurationSampleNum)
    {
        //进入到最新的train中：当pulsar silence or train silence结束后进入，如果两个silence都是0，则直接进入新的train
        if (changeTrainTrace && (currentState == why::PulsarStateEnum::IntraSilence || currentState ==
            why::PulsarStateEnum::InterTrainSilence || (commonVoiceSate->pulsarDutyCycleRatioParam->load() == 1 &&
                commonVoiceSate->trainSilenceParam->load() == 0)))
        {
            //按照新配来初始化
            resetTrain(newTrainDurationLen, newTrainIntervalSilenceLen, true, newTrainLen);
            changeTrainTrace = false;
        }

        changeStage(pulsarSamples, intraSilenceSamples, interTrainSilenceSamples, trainDutyCycleSamples);

        //如果某个状态的长度，为0个samples，那意味着当前process的过程应当自动跳入下个阶段
        //因为随着阶段的变更，一定会出现阶段长度不为0的部分，所以不会死循环
        while (currentStateDurationSampleNum == 0)
        {
            //继续change stage，以免无需存在的部分，占据sample，尽管是返回0值的sample
            changeStage(pulsarSamples, intraSilenceSamples, interTrainSilenceSamples, trainDutyCycleSamples);
        }


        pulsarAdsr.reset();
        //对pulse和silence都应用envelope，方便后续masking处理（如pulsar的silence转为pulse，这里不开启envelope就是0，那么转换后直接是0了）
        pulsarAdsr.noteOn();
        isTriggeredReleaseFlag = false;
    }

    bool passMaskFlag = true;
    bool existMask = false;
    mask(passMaskFlag, existMask);

    //曾经是pulsar silence，现在变成了pulse，需要用该长度确认pulsar的最终频率和adsr比例值
    bool silenceToPulseFlag = existMask && currentState == why::PulsarStateEnum::IntraSilence && passMaskFlag;

    //pulse duty cycle调制后的频率大小，将会影响pulse的长度
    float pulsarModFreq;
    calcNewPulsarFreq(pulsarModFreq, silenceToPulseFlag);

    // 修改adsr时间:注意经过mask后，silence可能也是pulse，要实时调整adsr的比例基准值，即用pulsar dutycycle还是pulsar silence的时长
    if (silenceToPulseFlag)
    {
        //应用于pulse（cluster）
        refreshPulsaretAdsr((1 - this->pulsarDutyCycleRatio) * this->pulsarPeriodTime);
    }
    else
    {
        refreshPulsaretAdsr((this->pulsarDutyCycleRatio) * this->pulsarPeriodTime);
    }
    //触发release阶段：注意在应用的时长内，要还有空间可以应用release否则不触发
    if (!isTriggeredReleaseFlag && pulsarAdsr.isActive() && currentStateDurationSampleNum -
        hasPassedSampleNumInsideTrain <= pulsarAdsrParams.release * getSampleRate())
    {
        pulsarAdsr.noteOff();
        isTriggeredReleaseFlag = true;
    }

    //计算当前状态的sample值
    float s = calSampleByState(passMaskFlag, existMask, pulsarModFreq);

    hasPassedSampleNumInsideTrain++;
    totalSampleCount++;

    // DBG(juce::String::formatted(
    //     "hasPassedSampleNumInsideTrain:%d ; totalSampleCount:%d ; sample:%.5f",
    //     (int)hasPassedSampleNumInsideTrain,
    //     (int)sinceLastTransitionSamplesTotal,
    //     s));
    return s;
}

float PulsarSynthVoice::calcActualPulse(float pulsarModFreq)
{
    //扫描sample播放
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

    float s =
        PulsaretWaveformSingleton::getInstance().calcSample(
            commonVoiceSate->pulsarWaveformParam->load(), pulsaretPhase
        ) * calcAmpLfoInterpolation(pulsarModFreq, commonVoiceSate->sustainParam->load()) * pulsarAdsr.getNextSample();
    //必须放在计算sample之后，而不是之前，否则起始点phase偏移了
    pulsaretPhase += pulsarModFreq / getSampleRate();
    // pulsaretPhase = std::fmod(pulsaretPhase, 1.0f); //这种没有减法快
    if (pulsaretPhase > 1.0)
    {
        pulsaretPhase = pulsaretPhase - 1.0;
    }

    return s;
}

float PulsarSynthVoice::calcFormantLfoInterpolation(float pulsarModFreq, float amount)
{
    //默认用fundamentalfreq
    if (commonVoiceSate->formantFreqLfoParam->load() <= 0.0)
    {
        return 0.0;
    }
    // 获取单例并设置频率
    LfoWaveformSingleton& lfo = LfoWaveformSingleton::getInstance(getSampleRate(), pulsarModFreq);
    return lfo.calcSampleAfterFM(commonVoiceSate->formantFreqLfoParam->load(), pulsaretPhase) * amount;
}

float PulsarSynthVoice::calcAmpLfoInterpolation(float pulsarModFreq, float amount)
{
    //默认用fundamentalfreq
    if (commonVoiceSate->ampLfoParam->load() <= 0.0)
    {
        return amount;
    }
    // 获取单例并设置频率
    LfoWaveformSingleton& lfo = LfoWaveformSingleton::getInstance(getSampleRate(), pulsarModFreq);
    return lfo.calcSampleAfterAM(commonVoiceSate->ampLfoParam->load(), pulsaretPhase) * amount;
}

float PulsarSynthVoice::getOutputGain()
{
    //db转为gain值返回
    return commonVoiceSate->outputGainParam == nullptr
               ? 1
               : std::pow(10.0f, commonVoiceSate->outputGainParam->load() / 20.0f);
}

int PulsarSynthVoice::refreshBpm(juce::AudioPlayHead* audioPlayHead)
{
    //不在DAW中运行，则为NULL
    if (auto* temp = audioPlayHead)
    {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (audioPlayHead->getCurrentPosition(posInfo) && why::bpm.load() != posInfo.bpm)
        {
            why::bpm.store(posInfo.bpm);
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
