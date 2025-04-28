//
// Created by Mr. Wang on 2025/4/20.
//

#ifndef SHAREDVOICESTATE_H
#define SHAREDVOICESTATE_H
#include "Commons.h"
#include "ConvolutionResource.h"

/**
 * 单个synth下的多个voice共享用一个CommonVoiceSate，用于表示相同的字段，如burstMask与voices是1对多的关系
 */
class CommonVoiceSate
{
public:
    //whether loop playback train or not
    bool isLoop = true;

    //===========================masking===========================
    //mask仅针对train duty cycle，如2个pulsar periods，则输出的是pulse, silence, pulse, silence
    //mask为101101，则输出pulse, silence, pulse, pulse
    //如果是3个pulsar periods，经过该mask处理后，就输出pulse, silence, pulse, pulse, silence, pulse

    //mask option: default is off
    why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
    //burst mask:如1001
    std::string burstMask = "";
    //euclid，比如10101就是steps=5,hits=3的结果
    std::string euclids;

    //===========================mapping parameter of plugin processor===========================
    //parameters to receive values from AudioProcessorValueTreeState，thread safe
    std::atomic<float>* outputGainParam;
    std::atomic<float>* playModeParam;
    //train duty cycle由多少个pulsar period组成
    std::atomic<float>* trainDutyCycleLenParam;
    //train interval silence由多少个pulsar period组成
    std::atomic<float>* trainSilenceParam;
    //train length：单位为1 beat，4表示1个bar
    //由train length, train dutycyle,train silence可确定pulsar period，fundamental frequency
    std::atomic<float>* trainLenParam;

    //pulsar duty cycle
    std::atomic<float>* pulsarDutyCycleRatioParam;
    //pulsaret waveform
    std::atomic<float>* pulsarWaveformParam;
    //一个pulse可分化为为多个pulse，如1个train只有1个pulsar period, 并且pulse ratio=0.5，那么pulse duty cyle==pulse silence
    //如果cluster设置为4，则原来是pulse, silence,现在则是(pulse, pulse, pulse, pulse)(占据原来1个pulse的长度), silence
    std::atomic<float>* pulsarDutyCycleClusterLenParam;

    //FM LFO:应用于单个pulse(如果cluster>0，那么就是应用cluster细分后的pulse上)
    std::atomic<float>* formantFreqLfoParam;
    //AM LFO:应用于单个pulse(如果cluster>0，那么就是应用cluster细分后的pulse上)
    std::atomic<float>* ampLfoParam;

    //TODO 设计问题，是否保持和lfo一致？ adsr：应用于单个pulse(如果cluster>0，也针对原始的pluse的长度应用adsr，即cluster下的多个pulse的总长度上应用adsr)
    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;
    std::atomic<float>* maskOptionParam;

    //generate euclid rhythm to use as a mask, like a special burst mask
    std::atomic<float>* euclidStepsParam;
    std::atomic<float>* euclidHitsParam;

    //stochastic mask: 仅与train duty cycle有关，为trainDutyCycle的2倍
    std::string stochasticMaskStr = "";

    //mask菜单选择
    std::atomic<float>* impulseSwitchParam;

    //===========================卷积===========================
    //用于存储impulse file数据，在初始化synth时初始化该字段，所有synth，所有voice都共享一个ConvolutionResource，所以使用shared_ptr
    std::shared_ptr<ConvolutionResource> convolutionResource;

    //=========pulse buffer：用来批量处理sample，然后可以一次性与impulse response做convolution=========
    juce::AudioBuffer<float> pulseBuffer;

    //多个voice可能同时生成修改stochastic mask，用锁保证依次执行，仅在train duty cycle变化后才生成
    std::mutex strMutex;
    float previousTrainDutyCycleLen4GenStocMask;

    //只会在初始化synth时和parameterChanged中（改变了train dutycycle length）触发
    void generateStochasticMask()
    {
        //midi模式下可能会多个voice来修改stochastic
        std::lock_guard<std::mutex> guard(strMutex);
        //generate random mask, 它的长度是train duty cycle（即pulsar period个数）长度的两倍
        int totalPeriodNum = trainDutyCycleLenParam->load();
        if (previousTrainDutyCycleLen4GenStocMask == totalPeriodNum)
        {
            return;
        }
        //控制长度，random mask超过64个就限制
        while (totalPeriodNum > 64)
        {
            totalPeriodNum = totalPeriodNum / 2;
        }
        int num = static_cast<int>(2 * totalPeriodNum);
        stochasticMaskStr = why::generateBinaryString(num);

        previousTrainDutyCycleLen4GenStocMask = totalPeriodNum;
    };

    //===========================TODO 使用采样作为pulsaret===========================
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
    float readHead = 0;
};
#endif //SHAREDVOICESTATE_H
