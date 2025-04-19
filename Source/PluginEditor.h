#pragma once

//#include <juce_audio_utils/gui/juce_AudioVisualiserComponent.h>
#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor, public juce::Timer
                                              //这里实现apvts监听，而不是Slider::Listener，因为automation这类参数变化，后者好像不会监听到
                                              , public juce::AudioProcessorValueTreeState::Listener

{
public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);

    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;

    void resized() override;

    // 更新波形的函数
    void updateWaveform(const juce::AudioBuffer<float>& buffer);
    void timerCallback() override;
    //监听参数变化
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    //文件选择
    void openFileChooser()
    {
        // 创建一个文件选择器
        chooser = std::make_unique<juce::FileChooser>("Select a file to upload",
                                                      juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                      "*.wav;*.mp3");

        //读取窗口所选文件
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& chooser)
                             {
                                 juce::File selectedFile = chooser.getResult();
                                 if (selectedFile.exists())
                                 {
                                     juce::Logger::writeToLog("File selected: " + selectedFile.getFullPathName());
                                     loadFileIntoSynth(selectedFile);
                                     //展示路径
                                     impulsePathTextEditor.setText(selectedFile.getFullPathName());

                                     //当前如果是随机模式则直接加载

                                     loadSampleImpulse();
                                 }
                             });
    }

    void loadSampleImpulse()
    {
        if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1)
        {
            return;
        }
        if (processorRef.get_last_sample_impulse_memory_block()->getSize() <= 0)
        {
            //当前还没记载采样，也要清空template播放的卷积
            processorRef.set_should_use_convolution(false);
            return;
        }
        processorRef.set_should_use_convolution(true);

        //更新impulse file
        processorRef.get_convolution()->loadImpulseResponse(
            processorRef.get_last_sample_impulse_memory_block()->getData(),
            processorRef.get_last_sample_impulse_memory_block()->getSize(),
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes); // 0 = full IR
    }

    void loadFileIntoSynth(const juce::File& file)
    {
        if (file.getSize() <= 0)
        {
            return;
        }
        // std::unique_ptr<juce::File> filePtr = std::make_unique<juce::File>(file);
        // processorRef.set_convolution_file(filePtr);
        juce::MemoryBlock memBlock;
        file.loadFileAsData(memBlock);
        std::unique_ptr<juce::MemoryBlock> memBlockPtr = std::make_unique<juce::MemoryBlock>(memBlock);
        processorRef.set_last_sample_impulse_memory_block(memBlockPtr);
    }

    void loadTemplateImpulse()
    {
        //当前如果是template模式则直接加载
        if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Template) + 1)
        {
            return;
        }
        if (processorRef.get_last_template_data_size() <= 0)
        {
            //当前不使用卷积
            processorRef.set_should_use_convolution(false);
            return;
        }
        processorRef.set_should_use_convolution(true);

        //避免move后指针内部的数据变空，因为buffer的数据被move了
        auto clonedBuffer = std::make_unique<juce::AudioBuffer<float>>(
            *processorRef.get_last_template_buffer()
        );

        processorRef.get_convolution()->loadImpulseResponse(
            std::move(*clonedBuffer),
            processorRef.get_last_template_data_size(),
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            juce::dsp::Convolution::Normalise::yes); // 0 = full IR
    }

    void buildImpulseComboboxNames();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    juce::AudioVisualiserComponent visualiser = juce::AudioVisualiserComponent(0); // 波形显示组件

    //自定义控件
    //train
    juce::Slider trainLenSlider;
    juce::Label trainLenLabel;

    juce::Slider trainDutyCycleLenSlider;
    juce::Label trainDutyCycleLenLabel;

    juce::Slider trainSilenceLenSlider;
    juce::Label trainSilenceLenLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainLenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainDutyCycleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainSilenceAttachment;

    //pulsar
    juce::Slider pulsarWaveformSlider;
    juce::Label pulsarWaveformLabel;

    juce::Slider pulsarDutyCycleClusterLenSlider;
    juce::Label pulsarDutyCycleClusterLenLabel;

    juce::Slider pulsarDutyCycleRatioSlider;
    juce::Label pulsarDutyCycleRatioLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarWaveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarDutyCycleClusterLenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarDutyCycleRatioAttachment;

    //lfo modulation
    juce::Slider ampLfoSlider;
    juce::Label ampLfoLabel;

    juce::Slider formantFreqLfoSlider;
    juce::Label formantFreqLfoLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ampLfoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantFreqLfoAttachment;

    //envelope
    juce::Slider attackSlider;
    juce::Label attackLabel;

    juce::Slider decaySlider;
    juce::Label decayLabel;

    juce::Slider sustainSlider;
    juce::Label sustainLabel;

    juce::Slider releaseSlider;
    juce::Label releaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    //masking
    juce::Label maskComboBoxLabel;
    juce::ComboBox maskOptionComboBox;

    juce::Label burstMaskLabel;
    juce::TextEditor burstMaskTextEditor;

    juce::Label euclidStepLabel;
    juce::Slider euclidStepDial;

    juce::Label euclidHitLabel;
    juce::Slider euclidHitDial;

    juce::Label stochasticMaskLabel;
    juce::TextEditor stochasticMaskTextEditor;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maskOptionComboBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidStepAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidHitAttachment;

    //convolution
    juce::Label impulseSwitchLabel;
    juce::ComboBox impulseSwitchComboBox; //0禁用 1使用
    juce::TextEditor impulsePathTextEditor; //impulse file path

    juce::TextButton selectImpulseButton;
    juce::Label impulseTemplateLabel;
    juce::ComboBox impulseTemplateComboBox;

    juce::Label outputGainLabel;
    juce::Slider outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> impulseSwitchComboBoxAttachment;

    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainLenAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    //默认的binarydata文件名
    std::map<juce::String, juce::String> binaryIdFileNameMap;

    AudioPluginAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
