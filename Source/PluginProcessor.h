#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PulsarSynthEngine.h"

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
    void initOldAndNewParamMap();

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;

    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================自定义==============================
    //参数变化
    using APVTS = juce::AudioProcessorValueTreeState;
    juce::AudioProcessorValueTreeState apvts;

    /**
     * 监听控件参数变化，注意：
     * 1，automation不会触发这里，会直接修改apvts中的参数值
     * 2，getRawParameterValue() or getParameter() methods is not guaranteed to return the up-to-date value but newValue is
     *
     * @param parameterID parameter id
     * @param newValue up-to-date value
     */
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    /**
    * 手动监听parameter变化，并触发parameterChanged监听函数
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

    [[nodiscard]] std::map<juce::String, std::atomic<float>*>& getParamMap()
    {
        return paramMap;
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

    //用于实现手动触发parameterChanged监听：因插件窗口未打开时，automation只会自动修改apvts的参数，不触发parameterChanged监听，
    //目前我通过该监听修改的参数和同步ui控件
    std::map<juce::String, std::atomic<float>*> paramMap;
    std::map<juce::String, float> oldParamMap;

    //是否触发了load preset（一处写即processor中写，全局读，所以线程安全）
    bool loadingPresetFlag = false;

    //用于构建不同两个synth，从而实现对应不同的播放模式
    PulsarSynthEngine pulsarSynthEngine;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
