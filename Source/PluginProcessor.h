#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PulsarSynth.h"

//==============================================================================
//增加parameter变化监听
class AudioPluginAudioProcessor final : public juce::AudioProcessor
    // , public juce::ValueTree::Listener
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

    using APVTS = juce::AudioProcessorValueTreeState;
    juce::AudioProcessorValueTreeState apvts;

    //参数变化
    // void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    [[nodiscard]] std::unique_ptr<PulsarSynth>& get_pulsar()
    {
        return pulsar;
    }

    void set_pulsar(std::unique_ptr<PulsarSynth>& pulsar)
    {
        this->pulsar = std::move(pulsar);
    }

    [[nodiscard]] std::unique_ptr<juce::dsp::Convolution>& get_convolution()
    {
        return convolution;
    }

    void set_convolution(std::unique_ptr<juce::dsp::Convolution> convolution)
    {
        this->convolution = std::move(convolution);
    }

    [[nodiscard]] std::unique_ptr<juce::AudioBuffer<float>>& get_convolution_buffer()
    {
        return convolutionBuffer;
    }

    void set_convolution_buffer(std::unique_ptr<juce::AudioBuffer<float>>& convolution_buffer)
    {
        convolutionBuffer = std::move(convolution_buffer);
    }

    [[nodiscard]] std::unique_ptr<juce::File>& get_convolution_file()
    {
        return convolutionFile;
    }

    void set_convolution_file(std::unique_ptr<juce::File>& convolution_file)
    {
        convolutionFile = std::move(convolution_file);
    }

    [[nodiscard]] std::unique_ptr<juce::MemoryBlock>& get_last_sample_impulse_memory_block()
    {
        return lastSampleImpulseMemoryBlock;
    }

    void set_last_sample_impulse_memory_block(std::unique_ptr<juce::MemoryBlock>& convolution_memory_block)
    {
        lastSampleImpulseMemoryBlock = std::move(convolution_memory_block);
    }

    [[nodiscard]] std::unique_ptr<juce::AudioBuffer<float>>& get_last_template_buffer()
    {
        return lastTemplateBuffer;
    }

    void set_last_template_buffer(std::unique_ptr<juce::AudioBuffer<float>>& last_template_buffer)
    {
        lastTemplateBuffer = std::move(last_template_buffer);
    }

    [[nodiscard]] double& get_last_template_data_size()
    {
        return lastTemplateDataSize;
    }

    void set_last_template_data_size(double last_template_data_size)
    {
        lastTemplateDataSize = last_template_data_size;
    }

    [[nodiscard]] bool& is_should_use_convolution()
    {
        return shouldUseConvolution;
    }

    void set_should_use_convolution(bool should_use_convolution)
    {
        shouldUseConvolution = should_use_convolution;
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
    //存储采样的buffer
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

    std::unique_ptr<PulsarSynth> pulsar = std::make_unique<PulsarSynth>();

    std::unique_ptr<juce::AudioFormatReader> audioFormatReader;
    //负责格式
    std::unique_ptr<juce::AudioFormatManager> audioFormatManager = std::make_unique<juce::AudioFormatManager>();
    std::unique_ptr<juce::dsp::Convolution> convolution = std::make_unique<juce::dsp::Convolution>();
    //卷积采样
    std::unique_ptr<juce::AudioBuffer<float>> convolutionBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
    std::unique_ptr<juce::File> convolutionFile = std::make_unique<juce::File>();

    bool convolutionSetupFlag = false;

    //最后一次选择impulse sample的block
    std::unique_ptr<juce::MemoryBlock> lastSampleImpulseMemoryBlock = std::make_unique<juce::MemoryBlock>();
    //最后一次选择的impulse buffer
    std::unique_ptr<juce::AudioBuffer<float>> lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

    double lastTemplateDataSize = 0;
    bool shouldUseConvolution = false;

    double lastTick=-1;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        //输出增益：db
        params.push_back(
            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("outputGain", 1), "Output Gain", -60.0, 6.0, 0.0)
        );
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("impulseSwitch", 1), // parameter ID
            "Impulse Switch", // parameter name
            why::get_impulse_switch_array(),
            0 // 默认选择 index，combobox的id不为0，但这里是从0开始算
        ));

        //train长度
        params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("trainLen", 1), "Train Length (1/64)", 1, 64 * 16, 1.0)
        );
        //train duty cycle: how many numbers of pulsar period
        params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("trainDutyCycleLen", 1), "Train Duty Cycle Len", 1, 1000, 0)
        );
        //train silence：个数，改为缩放
        params.push_back(
            std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("trainSilenceLen", 1), "Train Silence", 0, 1000, 0)
        );

        //masking
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("maskOption", 1), // parameter ID
            "Mask Mode", // parameter name
            why::get_mask_option_array(),
            0 // 默认选择 index
        ));

        //欧几里得节奏划分pattern（train duty cycle）
        params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("euclidSteps", 1), "Euclid Steps", 0, 16, 0)
        );
        params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("euclidHits", 1), "Euclid Hits", 0, 16, 0)
        );

        //pulsarWaveform
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarWaveform", 1), "Pulsar Waveform", 0.0, 1.0, 0.0)
        );

        //pulsar duty cycle len
        params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID("pulsarDutyCycleClusterLen", 1), "Pulsar Duty Cycle Len", 1, 32, 1)
        );
        //pulsar duty cycle ratio，比例值
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarDutyCycleRatio", 1), "Pulsar Duty Cycle Ratio", 0.01, 1.0, 0.5)
        );

        //modulation
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("ampLfoWaveform", 1), "Amp Lfo Waveform", 0.0, 1.0, 0.0)
        );
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("formantFreqLfoWaveform", 1), "Formant Freq Waveform", 0.0, 1.0, 0.0)
        );

        //envelope
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarAttack", 1), "Pulsar Attack", 0.0, 1.0, 0.0001)
        );
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarDecay", 1), "Pulsar Decay", 0.0, 1.0, 0.0001)
        );
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarSustain", 1), "Pulsar Sustain", 0.0, 1.0, 1.0)
        );
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pulsarRelease", 1), "Pulsar Release", 0.0, 1.0, 0.0001)
        );

        return {params.begin(), params.end()};
    }
};
