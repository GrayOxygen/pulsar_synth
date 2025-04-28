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
/**
 * Commons维护一些公用的状态和简单的通用方法
 */
namespace why
{
    //定义全局的速度：是插件的速度而非DAW的速度，该插件仅受插件速度影响
    extern std::atomic<float> bpm;
    // extern std::atomic<double> sampleRate;

    /**
     * pulse状态，准确的说当前pulsar train所处于哪个阶段
     */
    enum class PulsarStateEnum
    {
        Pulse,
        IntraSilence, //silence inside one pular period
        InterTrainSilence //silence of train interval
    };

    /**
     * impulse switch下拉框选项
     */
    enum class ImpulseSwitchEnum
    {
        Off = 0,
        Template = 1,
        Sample = 2
    };

    /**
     * mask下拉框选项
     */
    enum class MaskOptionEnum
    {
        Off = 0,
        BurstMask = 1,
        EuclidMask = 2,
        StochasticMask = 3
    };

    /**
     * 播放模式
     */
    enum class PlayModeEnum
    {
        NotSelected = 0, //no sound
        Auto = 1, //triggered by playback of DAW
        Midi = 2 //triggered by MIDI note
    };

    /**
     * audio parameter name
     */
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

    //Some UI elements cannot be bound to audio parameters, so preset and UI updates cannot be automated.
    //Instead, the state is saved manually using properties for updates.
    struct PropertyID
    {
        static constexpr const char* burstMask = "burstMask";
        static constexpr const char* stochasticMask = "stochasticMask";
        static constexpr const char* sampleImpulsePath = "sampleImpulsePath";
        // static constexpr const char* currentPlayModeEnum = "currentPlayModeEnum";
    };

    /**
     * c++枚举不支持像java那样直接获取枚举的全部name，所以用单独的方法解决
     * @return impulse switch option names
     */
    juce::StringArray getImpulseSwitchArray();

    /**
     * c++枚举不支持像java那样直接获取枚举的全部name，所以用单独的方法解决
     * @return mask option names
     */
    juce::StringArray getMaskOptionArray();

    /**
     * genreate euclid rhythm
     *
     * 5,3 is 1 1 1,0 0 then 10 10 1
     * 5,2 is 10 10,0 then 10100
     * 7,3 is 10 10,0 0 0 then 100 100,0 then 1001000
     *
     * @param steps rhythm length
     * @param hits how many divides
     * @return
     */
    std::string generateEuclidRhythm(int steps, int hits);
    /**
     * 设置字符串中随机的某个字符为1
     * @param binaryString 包含0，,1的字符串
     */
    void setRandomIndexToOne(std::string& binaryString);
    /**
     * 生成指定长度仅包含0,1的字符串
     * @param length 指定长度
     * @return 如00011101
     */
    std::string generateBinaryString(int length);

    /**
     * c++枚举不支持像java那样直接获取枚举的全部name，所以用单独的方法解决
     * @return play modes
     */
    juce::StringArray getPlayModeArray();
    /**
     * 获取当前thread id
     * @return 当前thread id
     */
    std::string getThreadIdStr();
    bool readFileFromResources(const char* resourceName, double& sampleRate,
                               std::unique_ptr<juce::AudioBuffer<float>>& bf);
}
