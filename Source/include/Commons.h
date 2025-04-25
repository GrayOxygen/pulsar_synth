//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include <vector>
#include <ostream>
#include <sstream>
#include <string>
#include <random>
#include "JuceHeader.h"

namespace why
{
    // 声明原子类型的全局变量
    extern std::atomic<float> bpm;
    extern std::atomic<double> sampleRate;

    enum class PulsarStateEnum
    {
        Pulse,
        IntraSilence, //silence inside one pular cycle
        InterTrainSilence //silence of train interval
    };

    enum class ImpulseSwitchEnum
    {
        Off = 0,
        Template = 1,
        Sample = 2
    };

    enum class MaskOptionEnum
    {
        Off = 0,
        BurstMask = 1,
        EuclidMask = 2,
        StochasticMask = 3
    };

    enum class PlayModeEnum
    {
        Auto = 0,
        Midi = 1
    };

    struct ParameterID
    {
        static constexpr const char* outputGain = "outputGain";
        static constexpr const char* bpm = "bpm";
        static constexpr const char* impulseSwitch = "impulseSwitch";
        static constexpr const char* impulseTemplateFile = "impulseTemplateFile";
        static constexpr const char* trainLen = "trainLen";
        static constexpr const char* trainDutyCycleLen = "trainDutyCycleLen";
        static constexpr const char* trainSilenceLen = "trainSilenceLen";
        static constexpr const char* maskOption = "maskOption";
        static constexpr const char* euclidSteps = "euclidSteps";
        static constexpr const char* euclidHits = "euclidHits";
        static constexpr const char* pulsarWaveform = "pulsarWaveform";
        static constexpr const char* pulsarDutyCycleClusterLen = "pulsarDutyCycleClusterLen";
        static constexpr const char* pulsarDutyCycleRatio = "pulsarDutyCycleRatio";
        static constexpr const char* ampLfoWaveform = "ampLfoWaveform";
        static constexpr const char* formantFreqLfoWaveform = "formantFreqLfoWaveform";
        static constexpr const char* pulsarAttack = "pulsarAttack";
        static constexpr const char* pulsarDecay = "pulsarDecay";
        static constexpr const char* pulsarSustain = "pulsarSustain";
        static constexpr const char* pulsarRelease = "pulsarRelease";
        static constexpr const char* playMode = "playMode";
    };

    //有些控件设置attachment，所以没法绑定上juce的parameter，于是通过property进行绑定，load preset也能手动获取property值进行更新页面
    struct PropertyID
    {
        //没有出现在parameter定义中的参数，作为property
        //burst masking字符串
        static constexpr const char* burstMask = "burstMask";
        static constexpr const char* stochasticMask = "stochasticMask";
        static constexpr const char* sampleImpulsePath = "sampleImpulsePath";
        //用来保存当前播放模式，可以跨processor传递（复制track时，新建processor会setstateinformation，从而我们可以更新）
        static constexpr const char* currentPlayModeEnum = "currentPlayModeEnum";
    };


    juce::StringArray getImpulseSwitchArray();

    juce::StringArray getMaskOptionArray();

    std::string generateEuclidRhythm(int steps, int pulses);

    void setRandomIndexToOne(std::string& binaryString);

    std::string generateBinaryString(int length);

    juce::StringArray getPlayModeArray();

    std::string getThreadIdStr();
    juce::StringArray getEmptyChoiceArray();
    bool readFileFromResources(const char* resourceName, double& sampleRate,
                               std::unique_ptr<juce::AudioBuffer<float>>& bf);
}
