#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include "PulsarSynth.h"

void PulsarSynth::setSampleRate(double sampleRate)
{
    this->sampleRate = sampleRate;
}

void PulsarSynth::setPulsarSilence(float dutyCylce)
{
    pulsarDutyCycleRatio = dutyCylce;
    pulsarSilence = pulsarPeriodTime * (1 - pulsarDutyCycleRatio);
}

void PulsarSynth::setTrainAndPulsarDutyCycle(int durationLen, int intervalSilenceLen, float isLoop, int trainLen)
{
    //train长度由BPM确定，则可以计算缩放后的pulsar period
    if (intervalSilenceLen + durationLen <= 0)
    {
        return;
    }
    trainLenBlock = (60.0 / bpm) / 16;
    trainTime = trainLen * trainLenBlock;
    pulsarPeriodTime = trainTime / (intervalSilenceLen + durationLen);
    fundamentalFreq = 1.0 / pulsarPeriodTime;
    trainSilenceTime = intervalSilenceLen * pulsarPeriodTime;
    trainDutyCycleTime = durationLen * pulsarPeriodTime;

    //pulsar time ( = n*(pulsar duration+pulsar silence) ) + train interval silence
    trainPeriodTime = trainDutyCycleTime + trainSilenceTime;
    //必须满足trainDutyCycle / freq > pulsarDutyCycleRatio / fundamentalFreq
}

void PulsarSynth::buildTrain()
{
    setTrainAndPulsarDutyCycle(static_cast<int>(trainDutyCycleLenParam->load()), trainSilenceParam->load(), true,
                               trainLenParam->load());
    setPulsarSilence(pulsarDutyCycleRatioParam->load());
    //更新adsr
    refreshAdsr(pulsarDutyCycleRatio * pulsarPeriodTime);
}

void PulsarSynth::init(float sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                       juce::AudioPlayHead* audio_play_head)
{
    build_bpm(audio_play_head);
    this->sampleRate = sampleRate;
    this->sampleBuffer = std::move(sampleBuffer);
    DBG("HOW MANY TIMES" <<why::get_thread_id_str());
    initWaves();
    buildTrain();

    //根据train长度生成对应随机mask
    generateRandomMask();
}

void PulsarSynth::initWaves()
{
    this->sineLUT.initialise(
        [](float x) { return std::sin(2.0f * juce::MathConstants<float>::pi * x); },
        -1.0f, 1.0f, 1024
    );
    this->sawLUT.initialise(
        [](float x) { return 2.0f * (x - std::floor(x + 0.5f)); },
        -1.0f, 1.0f, 1024
    );
    this->squareLUT.initialise(
        [](float x) { return (x < 0.5f) ? -1.0f : 1.0f; },
        -1.0f, 1.0f, 1024
    );
    this->triangleLUT.initialise(
        [](float x) { return 2.0f * abs(2.0f * (x - std::floor(x + 0.5f))) - 1.0f; },
        -1.0f, 1.0f, 1024
    );
    this->pwmLUT.initialise(
        [](float x)
        {
            // 产生一个正弦波调制的方波
            return (x < 0.5f) ? std::sin(2.0f * juce::MathConstants<float>::pi * x) : 0.0f;
        },
        -1.0f, 1.0f, 1024
    );
    this->noiseLUT.initialise(
        [](float x)
        {
            return (rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f; // 生成-1到1之间的噪声
        },
        -1.0f, 1.0f, 1024
    );
    this->complexWaveLUT.initialise(
        [](float x)
        {
            // 生成多个频率的正弦波叠加
            float wave = std::sin(2.0f * juce::MathConstants<float>::pi * x);
            wave += 0.5f * std::sin(4.0f * juce::MathConstants<float>::pi * x); // 和谐波叠加
            wave += 0.25f * std::sin(8.0f * juce::MathConstants<float>::pi * x);
            return wave;
        },
        -1.0f, 1.0f, 1024
    );

    waveformLUTs.push_back(&sineLUT);
    waveformLUTs.push_back(&triangleLUT);
    waveformLUTs.push_back(&sawLUT);
    waveformLUTs.push_back(&squareLUT);
    waveformLUTs.push_back(&pwmLUT);
    waveformLUTs.push_back(&complexWaveLUT);
    waveformLUTs.push_back(&noiseLUT);

    //自己维护phase（取值[0,1)）
    auto makeTriangle = [](float x)
    {
        float normX = x / juce::MathConstants<float>::twoPi; // 映射到 [0, 1]
        return 2.0f * std::abs(2.0f * (normX - std::floor(normX + 0.5f))) - 1.0f;
    };

    auto makeSaw = [](float x)
    {
        return juce::jmap(x, 0.0f, juce::MathConstants<float>::twoPi, -1.0f, 1.0f);
    };

    auto makeSquare = [](float x)
    {
        return (x < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
    };

    auto makeSine = [](float x)
    {
        return std::sin(x); // JUCE 默认输入是 [0, 2π]
    };

    // Amp LFOs
    ampLfoSine.prepare({sampleRate, 512, 1});
    ampLfoSine.setFrequency(fundamentalFreq);
    ampLfoSine.initialise(makeSine);

    ampLfoSaw.prepare({sampleRate, 512, 1});
    ampLfoSaw.setFrequency(fundamentalFreq);
    ampLfoSaw.initialise(makeSaw);

    ampLfoSquare.prepare({sampleRate, 512, 1});
    ampLfoSquare.setFrequency(fundamentalFreq);
    ampLfoSquare.initialise(makeSquare);

    ampLfoTriangle.prepare({sampleRate, 512, 1});
    ampLfoTriangle.setFrequency(fundamentalFreq);
    ampLfoTriangle.initialise(makeTriangle);

    ampLfos.push_back(&ampLfoSine);
    ampLfos.push_back(&ampLfoTriangle);
    ampLfos.push_back(&ampLfoSaw);
    ampLfos.push_back(&ampLfoSquare);

    // Formant Freq LFOs
    formantFreqLfoSine.prepare({sampleRate, 512, 1});
    formantFreqLfoSine.setFrequency(fundamentalFreq);
    formantFreqLfoSine.initialise(makeSine);

    formantFreqLfoSaw.prepare({sampleRate, 512, 1});
    formantFreqLfoSaw.setFrequency(fundamentalFreq);
    formantFreqLfoSaw.initialise(makeSaw);

    formantFreqLfoSquare.prepare({sampleRate, 512, 1});
    formantFreqLfoSquare.setFrequency(fundamentalFreq);
    formantFreqLfoSquare.initialise(makeSquare);

    formantFreqLfoTriangle.prepare({sampleRate, 512, 1});
    formantFreqLfoTriangle.setFrequency(fundamentalFreq);
    formantFreqLfoTriangle.initialise(makeTriangle);

    formantLfos.push_back(&formantFreqLfoSine);
    formantLfos.push_back(&formantFreqLfoTriangle);
    formantLfos.push_back(&formantFreqLfoSaw);
    formantLfos.push_back(&formantFreqLfoSquare);

    //pulsar envelope
    adsr.setSampleRate(sampleRate);

    adsrParams.attack = attackParam->load();
    adsrParams.decay = decayParam->load();
    adsrParams.sustain = sustainParam->load();
    adsrParams.release = releaseParam->load();

    adsr.setParameters(adsrParams);
}

void PulsarSynth::generateRandomMask()
{
    //generate random mask, 它的长度是train duty cycle（即pulsar period个数）长度的两倍
    int totalPeriodNum = trainDutyCycleLenParam->load();
    //控制长度，random mask超过64个就限制
    while (totalPeriodNum > 64)
    {
        totalPeriodNum = totalPeriodNum / 2;
    }
    int num = static_cast<int>(2 * totalPeriodNum);
    stochasticMaskStr = why::generateBinaryString(num);
}

void PulsarSynth::connectParameters(juce::AudioProcessorValueTreeState& apvts)
{
    mappingParams(apvts);

    // setTrainAndPulsarDutyCycle(static_cast<int>(trainDutyCycleLenParam->load()), trainSilenceParam->load(), true,
    //                            trainLenParam->load());
    //
    // //根据train长度生成对应随机mask
    // generateRandomMask();
    //
    // setPulsarSilence(pulsarDutyCycleRatioParam->load());
    // //应用于单个pulsar
    // refreshAdsr(pulsarDutyCycleRatio * pulsarPeriodTime);
}

void PulsarSynth::mappingParams(const juce::AudioProcessorValueTreeState& apvts)
{
    //volume
    this->outputGainParam = apvts.getRawParameterValue("outputGain");

    //train
    this->trainLenParam = apvts.getRawParameterValue("trainLen");
    this->trainDutyCycleLenParam = apvts.getRawParameterValue("trainDutyCycleLen");
    this->trainSilenceParam = apvts.getRawParameterValue("trainSilenceLen");

    //pulsar basic info
    this->pulsarWaveformParam = apvts.getRawParameterValue("pulsarWaveform");
    this->pulsarDutyCycleClusterLenParam = apvts.getRawParameterValue("pulsarDutyCycleClusterLen");
    this->pulsarDutyCycleRatioParam = apvts.getRawParameterValue("pulsarDutyCycleRatio");

    //pulsar modulation
    this->formantFreqLfoParam = apvts.getRawParameterValue("formantFreqLfoWaveform");
    this->ampLfoParam = apvts.getRawParameterValue("ampLfoWaveform");

    //pulsar envelope
    this->attackParam = apvts.getRawParameterValue("pulsarAttack");
    this->decayParam = apvts.getRawParameterValue("pulsarDecay");
    this->sustainParam = apvts.getRawParameterValue("pulsarSustain");
    this->releaseParam = apvts.getRawParameterValue("pulsarRelease");

    //masking
    this->maskOptionParam = apvts.getRawParameterValue("maskOption");
    maskOption = static_cast<why::MaskOptionEnum>(static_cast<int>(maskOptionParam->load()));

    //texteditor必须通过property传递，没有attachment直接绑定
    if (!apvts.state.getProperty("burstMask").isVoid())
    {
        //get texteditor value from property because juce can't bind texteditor with parameter automatally
        burstMask = apvts.state.getProperty("burstMask").toString().toStdString();
    }

    //euclid
    this->euclidStepsParam = apvts.getRawParameterValue("euclidSteps");
    this->euclidHitsParam = apvts.getRawParameterValue("euclidHits");

    if (this->euclidStepsParam->load() > 0 && this->euclidHitsParam->load() > 0)
    {
        //generate euclid rhythm pattern
        this->euclids = why::euclidean_rhythm(this->euclidStepsParam->load(), this->euclidHitsParam->load());
    }

    impulseSwitch = static_cast<int>(apvts.getRawParameterValue("impulseSwitch")->load()) != static_cast<int>(
        why::ImpulseSwitchEnum::Off);
}

//针对单个参数调节
void PulsarSynth::parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID)
{
    mappingParams(apvts);

    //TODO 如果train参数变更，则当前运行train必须跑完再进入，可能不实现
    if (parameterID == "trainDutyCycleLen" || parameterID == "trainSilenceLen" || parameterID == "trainLen")
    {
        setTrainAndPulsarDutyCycle(static_cast<int>(trainDutyCycleLenParam->load()), trainSilenceParam->load(), true,
                                   trainLenParam->load());
        if (parameterID != "trainSilenceLen")
        {
            //根据train长度生成对应随机mask
            generateRandomMask();
        }
    }

    //刷新pulsar
    setPulsarSilence(pulsarDutyCycleRatioParam->load());
    refreshAdsr(pulsarDutyCycleRatio * pulsarPeriodTime);
}

void PulsarSynth::refreshAdsr(float pulsarTime)
{
    adsrParams.attack = pulsarTime * attackParam->load();
    adsrParams.decay = pulsarTime * decayParam->load();
    adsrParams.sustain = sustainParam->load();
    adsrParams.release = pulsarTime * releaseParam->load();
    adsr.setParameters(adsrParams);
}

void PulsarSynth::mask(bool& passFlag, bool& existMask)
{
    if (maskOption == why::MaskOptionEnum::Off)
    {
        existMask = false;
        return;
    }
    //burst masking
    if (maskOption == why::MaskOptionEnum::BurstMask && pulsarChangeCounter > 0 && !burstMask.empty())
    {
        existMask = true;
        //如果设置了masking，则重新设置
        int index = fmod(pulsarChangeCounter - 1, burstMask.size()) - 1;
        passFlag = burstMask[index] == '1';
    }
    //euclid masking
    if (maskOption == why::MaskOptionEnum::EuclidMask && pulsarChangeCounter > 0 && !euclids.empty())
    {
        existMask = true;
        //如果设置了masking，则重新设置
        int index = fmod(pulsarChangeCounter - 1, euclids.size());
        passFlag = euclids[index] == '1';
    }
    //stochastic masking
    if (maskOption == why::MaskOptionEnum::StochasticMask && pulsarChangeCounter > 0 && !stochasticMaskStr.empty())
    {
        //每次修改train lenth，pulsar个数等都修改stochastic mask
        existMask = true;
        int index = fmod(pulsarChangeCounter - 1, stochasticMaskStr.size());
        passFlag = stochasticMaskStr[index] == '1';
    }
}

float PulsarSynth::processSample()
{
    //once playback
    if (!isLoop && trainCounter >= 1)
    {
        return 0;
    }

    sinceLastTransitionSamples++;

    if (trainDutyCycleTime <= 0)
    {
        return 0.0;
    }

    int pulsarPeriodSamples = pulsarPeriodTime * sampleRate;
    int pulsarSamples = pulsarDutyCycleRatio * pulsarPeriodTime * sampleRate;
    int intraSilenceSamples = (1 - pulsarDutyCycleRatio) * pulsarPeriodTime * sampleRate;
    int interTrainSilenceSamples = trainSilenceTime * sampleRate;
    int trainDutyCycleSamples = trainDutyCycleTime * sampleRate;

    // 状态切换：每次达到当前状态的时间最大值，就会进入下一个阶段
    if (sinceLastTransitionSamples >= currentStateDurationSamples)
    {
        sinceLastTransitionSamples = 0;

        //更新状态，train的位置等信息
        switch (currentState)
        {
        case why::StateEnum::Pulse: //当前已走完pulsar的duty cycle阶段
            //关闭envelope
            adsr.noteOff();
        //检查是否还有时间继续这个train
            if (trainPositionSamples + intraSilenceSamples <= trainDutyCycleSamples)
            {
                currentState = why::StateEnum::IntraSilence;
                currentStateDurationSamples = intraSilenceSamples;
                //新状态对应的总历时
                trainPositionSamples += intraSilenceSamples;

                pulsarChangeCounter++;
                //对pulse和silence都应用envelope，方便后续masking处理（如pulsar的silence转为pulse，这里不开启envelope就是0，那么转换后直接是0了）
                adsr.noteOn();
            }
            else
            {
                // train结束，进入train间静音
                currentState = why::StateEnum::InterTrainSilence;
                //剩余不再继续进行pulsar period的部分，则直接silence，然后再加上下个周期时长
                currentStateDurationSamples =
                    (trainDutyCycleSamples - trainPositionSamples) + interTrainSilenceSamples;
            }
            break;
        case why::StateEnum::IntraSilence: //当前已走完pulsar的silence阶段
            adsr.noteOff();
        // 检查是否还能容纳下一个完整周期
            if ((trainPositionSamples + pulsarSamples) <= trainDutyCycleSamples)
            {
                currentState = why::StateEnum::Pulse;
                ++pulsarCounter;
                currentStateDurationSamples = pulsarSamples;
                trainPositionSamples += pulsarSamples;

                //开启envelope
                adsr.noteOn();

                pulsarChangeCounter++;
            }
            else
            {
                // 剩余时间不足完整周期，直接结束train
                currentState = why::StateEnum::InterTrainSilence;
                //剩余不再继续进行pulsar period的部分，则直接silence，再加上train interval time
                currentStateDurationSamples =
                    (trainDutyCycleSamples - trainPositionSamples) + interTrainSilenceSamples;
                trainPositionSamples = 0;
            }
            break;
        case why::StateEnum::InterTrainSilence: //train结束后，在train和train之间的silence阶段
            currentState = why::StateEnum::Pulse;
            pulsarCounter++;
            trainCounter++;
            currentStateDurationSamples = pulsarSamples;
        //走完了train duty cycle + train interval silence则表示走完了一个train
        //重置抵达下一个位置的samples
            trainPositionSamples = pulsarSamples;
        //开启envelope
            adsr.noteOn();

            pulsarChangeCounter = 1;
            break;
        }
    }

    bool passMaskFlag = true;
    bool existMask = false;
    mask(passMaskFlag, existMask);

    //pulse duty cycle调制后的频率大小，将会影响pulse的长度
    float pulsarModFreq = (
        (pulsarDutyCycleClusterLenParam->load() * 1.0 / (pulsarDutyCycleRatio * pulsarPeriodTime))
    ) + calcFormantLfoInterpolation();

    //修改adsr时间
    refreshAdsr(1.0 / pulsarModFreq);
    float s;
    // 生成当前样本
    switch (currentState)
    {
    case why::StateEnum::Pulse:
    case why::StateEnum::IntraSilence: //如果存在mask，则silence阶段可能发出声音，否则全是0
        if (!existMask && currentState == why::StateEnum::IntraSilence)
        {
            currentCacluatedPulse = false;
            return 0;
        }
        if (!passMaskFlag)
        {
            currentCacluatedPulse = false;
            return 0;
        }
        currentCacluatedPulse = true;
        s = calcActualPulse(pulsarModFreq);
        return s;
    case why::StateEnum::InterTrainSilence:
        currentCacluatedPulse = false;
        return 0.0f;
    }
    return 0.0f;
}

float PulsarSynth::calcActualPulse(float pulsarModFreq)
{
    pulsaretPhase += pulsarModFreq / sampleRate;
    pulsaretPhase = std::fmod(pulsaretPhase, 1.0f);
    //扫描sample播放
    float s;
    if (sampleBuffer != nullptr)
    {
        int index1 = static_cast<int>(readHead);
        int index2 = index1 + 1;
        float frac = readHead - index1;

        if (index1 >= 0 && index2 < sampleBuffer->getNumSamples())
        {
            float s1 = sampleBuffer->getReadPointer(0)[index1];
            float s2 = sampleBuffer->getReadPointer(0)[index2];
            s = (1.0f - frac) * s1 + frac * s2; // 线性插值
        }
        //根据当前pulse duty cycle长度，来判断sample需要以什么速度播放，从而确认以多少step读取samples
        //readHea按2个steps读取就是倍速，3个就是3倍速
        //pulse如果是2hz，就表示1秒2次，所以速度就2倍速
        //在指定时长内播完采样
        //调制后的频率得到新的duty cycle长度
        readHead += sampleBuffer->getNumSamples() / ((1.0 / pulsarModFreq) * sampleRate);
        readHead = std::fmod(readHead, sampleBuffer->getNumSamples());
        //扫描sample播放
    }
    //==============hann window实现每个pulse两头衰减为0==============
    // 计算衰减的起始和结束点
    float fadeStart = 0.95; // 衰减开始的位置
    float fadeEnd = 1.0f - fadeStart; // 衰减结束的位置
    // 计算窗口位置
    float hann = 1.0f; // 默认值为 1 (没有衰减)

    // 在衰减范围内计算窗口
    if (pulsaretPhase < fadeStart)
    {
        // 衰减部分（前 10%）
        hann = 0.5f *
            (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (pulsaretPhase / fadeStart)));
    }
    else if (pulsaretPhase > fadeEnd)
    {
        // 衰减部分（后 10%）
        hann = 0.5f * (1.0f - std::cos(
            2.0f * juce::MathConstants<float>::pi * ((1.0f - pulsaretPhase) / (1.0f - fadeEnd))));
    }
    //float hann = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * pulsaretPhase));

    //========================================================
    //return ((s + calcSample()) / 2.0) * calcAmpLfoInterpolation() * adsr.getNextSample() * hann;
    return calcSample() * calcAmpLfoInterpolation() * adsr.getNextSample();
}

float PulsarSynth::calcFormantLfoInterpolation()
{
    if (formantFreqLfoParam->load() <= 0.0)
    {
        return 0.0;
    }
    int size = formantLfos.size();
    float index = formantFreqLfoParam->load() * (size - 1);
    int lowerIndex = static_cast<int>(index);
    int upperIndex = std::min(lowerIndex + 1, size - 1);
    float factor = index - lowerIndex;
    float result = formantLfos[lowerIndex]->processSample(pulsaretPhase) +
        (formantLfos[upperIndex]->processSample(pulsaretPhase) -
            formantLfos[lowerIndex]->processSample(pulsaretPhase)) *
        factor;
    return result;
}

float PulsarSynth::calcAmpLfoInterpolation()
{
    if (ampLfoParam->load() <= 0.0)
    {
        return 1.0;
    }
    int size = ampLfos.size();
    float index = ampLfoParam->load() * (size - 1);
    int lowerIndex = static_cast<int>(index);
    int upperIndex = std::min(lowerIndex + 1, size - 1);
    float factor = index - lowerIndex;
    float result = ampLfos[lowerIndex]->processSample(pulsaretPhase) +
        (ampLfos[upperIndex]->processSample(pulsaretPhase) -
            ampLfos[lowerIndex]->processSample(pulsaretPhase)) *
        factor;
    return result;
}

float PulsarSynth::calcSample()
{
    // 将slider值映射到waveformLUTs数组的两个相邻波形之间
    int numWaveforms = waveformLUTs.size();
    float index = pulsarWaveformParam->load() * (numWaveforms - 1);

    int lowerIndex = static_cast<int>(index); // 选择下一个波形的索引
    int upperIndex = std::min(lowerIndex + 1, numWaveforms - 1); // 选择上一个波形的索引，确保不越界

    // 计算插值因子
    float interpolationFactor = index - lowerIndex;

    // 获取对应位置的两个波形
    juce::dsp::LookupTableTransform<float>& lowerWaveform = *waveformLUTs[lowerIndex];
    juce::dsp::LookupTableTransform<float>& upperWaveform = *waveformLUTs[upperIndex];

    float sample = lowerWaveform.processSample(pulsaretPhase)
        + (upperWaveform.processSample(pulsaretPhase) -
            lowerWaveform.processSample(pulsaretPhase)) *
        interpolationFactor;
    return sample;
}

float PulsarSynth::get_output_gain()
{
    if (outputGainParam == nullptr)
    {
        return 1;
    }
    float gain = std::pow(10.0f, outputGainParam->load() / 20.0f);
    return gain;
}

void PulsarSynth::refresh_burst_mask(juce::String burstMask)
{
    this->burstMask = burstMask.toStdString();
}
