#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "include/PulsarSynthEngine.h"

//==============================================================================
//增加parameter变化监听
class AudioPluginAudioProcessor final : public juce::AudioProcessor
                                        // , public juce::ValueTree::Listener
                                        , public juce::ChangeBroadcaster // 广播消息
                                        , public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();

    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(int index) override;

    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;

    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================自定义==============================
    //参数变化
    using APVTS = juce::AudioProcessorValueTreeState;
    juce::AudioProcessorValueTreeState apvts;

    /**
     * Listen for changes in control parameters，注意：
     * getRawParameterValue() or getParameter() methods is not guaranteed to return the up-to-date value but newValue is
     *
     * @param parameterID parameter id
     * @param newValue up-to-date value
     */
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    /**
    * Manually monitor the changes of the parameter and trigger the parameterChanged monitoring function
    */
    void parameterChangedManually();

    [[nodiscard]] bool& isLoadingPresetFlag()
    {
        return loadingPresetFlag;
    }

    void setLoadingPresetFlag(bool loadingPresetFlag)
    {
        this->loadingPresetFlag = loadingPresetFlag;
    }

    [[nodiscard]] PulsarSynthEngine& getPulsarSynthEngine()
    {
        return pulsarSynthEngine;
    }

    void setPulsarSynthEngine(PulsarSynthEngine& pulsarSynthEngine)
    {
        this->pulsarSynthEngine = std::move(pulsarSynthEngine);
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)

    //Whether the load preset is triggered (one write in the processor and the global read, so it is thread-safe)
    bool loadingPresetFlag = false;

    //It is used to build two different Synths, thereby achieving corresponding different playback modes
    PulsarSynthEngine pulsarSynthEngine;

    //throttling: avoid invoke too much parameterchanged
    int64 lastChangeTime=0;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};