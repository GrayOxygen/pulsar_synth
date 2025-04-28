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
 * Commons maintains some common states and simple methods
 */
namespace why
{
    // Define the global speed: It is the speed of the plugin rather than the speed of the DAW.
    // This plugin is only affected by the speed of the plugin
    extern std::atomic<float> bpm;
    // extern std::atomic<double> sampleRate;

    /**
     * pulse state, precisely speaking, which stage the pulsar train is currently in
     */
    enum class PulsarStateEnum
    {
        Pulse,
        IntraSilence, //silence inside one pular period
        InterTrainSilence //silence of train interval
    };

    /**
     * impulse switch combobox option
     */
    enum class ImpulseSwitchEnum
    {
        Off = 0,
        Template = 1,
        Sample = 2
    };

    /**
     * mask combobox option
     */
    enum class MaskOptionEnum
    {
        Off = 0,
        BurstMask = 1,
        EuclidMask = 2,
        StochasticMask = 3
    };

    /**
     * play mode
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
     * c++ enumeration does not support directly obtaining all the names of the enumeration as in java,
     * so a separate method is used to solve it
     *
     * @return impulse switch option names
     */
    juce::StringArray getImpulseSwitchArray();

    /**
     * c++ enumeration does not support directly obtaining all the names of the enumeration as in java,
     * so a separate method is used to solve it
     *
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
     * Set a random character in the string to 1
     *
     * @param binaryString string contains only 0 and 1
     */
    void setRandomIndexToOne(std::string& binaryString);

    /**
     * Generate a string of the specified length that contains only 0 and 1
     *
     * @param length target length
     * @return 如00011101
     */
    std::string generateBinaryString(int length);

    /**
     * c++ enumeration does not support directly obtaining all the names of the enumeration as in java,
     * so a separate method is used to solve it
     *
     * @return play modes
     */
    juce::StringArray getPlayModeArray();
    /**
     * get current thread id
     * @return current thread id
     */
    std::string getThreadIdStr();
    bool readFileFromResources(const char* resourceName, double& sampleRate,
                               std::unique_ptr<juce::AudioBuffer<float>>& bf);
}
