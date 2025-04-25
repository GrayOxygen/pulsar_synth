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

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;

    void setStateInformation(const void* data, int sizeInBytes) override;

    //===自定义===
    //参数变化
    using APVTS = juce::AudioProcessorValueTreeState;
    juce::AudioProcessorValueTreeState apvts;

    //监听控件参数变化:在这里（音频线程）中调用juce::AudioProcessorValueTreeState的apvts.getRawParameterValue并非最新
    //解决方案：每次单个parameter更新时都更新全部parameters和用到的property
    //automation不会触发这里，会直接修改apvts中的参数值
    void parameterChanged(const juce::String& parameterID, float newValue);

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

    // //TODO 存储采样的buffer，准备废弃
    // std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

    //解决手动触发parameterChanged监听
    //指向apvts的参数
    std::map<juce::String, std::atomic<float>*> paramMap;
    std::map<juce::String, float> oldParamMap;

    //是否触发了load preset，一处写，全局读，所以线程安全
    bool loadingPresetFlag = false;

    PulsarSynthEngine pulsarSynthEngine;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
