//
// Created by Wang on 2025/4/25.
//

#ifndef SHAREDVOICESTATE_H
#define SHAREDVOICESTATE_H
#include "Commons.h"
#include "ConvolutionResource.h"

class CommonVoiceSate
{
public:
    //tain结束后是否循环
    bool isLoop = true;

    //===========================masking===========================
    why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
    //burst
    //比如111010表示pulse pulse pulse silence pulse silence
    //假设一个train有1个pulsar period的train duty cycle构成，2个pulsar period长的train silence构成
    //那么01101则表示原来的train duty cycle为pulse silence变为silence pulse
    //如果train duty cycle有两个pulsar period，即pulse silence pulse silence将变成silence pulse pulse silence
    std::string burstMask = "";
    //euclid
    //表示几个pulsar duty cycle的长度
    std::string euclids;

    //===========================mapping parameter of plugin processor===========================
    //parameters to receive values from AudioProcessorValueTreeState，线程安全
    std::atomic<float>* outputGainParam;
    std::atomic<float>* playModeParam;
    std::atomic<float>* trainDutyCycleLenParam;
    std::atomic<float>* trainSilenceParam;
    //1 beat为单位，4表示1个bar;   train len：多少个trainLenBlock
    std::atomic<float>* trainLenParam;

    std::atomic<float>* pulsarDutyCycleRatioParam;
    std::atomic<float>* pulsarWaveformParam;
    //一个pulse区域可以设置为多个pulse，长度以原本的pulse width为单位
    std::atomic<float>* pulsarDutyCycleClusterLenParam;

    std::atomic<float>* formantFreqLfoParam;
    std::atomic<float>* ampLfoParam;

    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;
    std::atomic<float>* maskOptionParam;

    //欧几里得节奏，也相当于一种mask
    std::atomic<float>* euclidStepsParam;
    std::atomic<float>* euclidHitsParam;

    //stochastic
    std::string stochasticMaskStr = "";

    //mask菜单选择
    std::atomic<float>* impulseSwitchParam;

    //===========================TODO 扫描采样，采样作为pulsaret===========================
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
    float readHead = 0;

    //===========================卷积===========================
    //初始voice时初始convolution，共享synth同一个ConvolutionResource
    //线程安全，因为只有一处写（由plugineditor触发在PulsarSynth类中修改，无写的并发），其余都是读
    std::shared_ptr<ConvolutionResource> convolutionResource;

    //===========================pulse buffer：用来批量处理convolution===========================
    juce::AudioBuffer<float> pulseBuffer;
};
#endif //SHAREDVOICESTATE_H
