#pragma once

//#include <juce_audio_utils/gui/juce_AudioVisualiserComponent.h>
#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
                                              // , public juce::Timer
                                              // 这里实现apvts监听，不要用Slider::Listener，因为不会监听到automation这类参数变化
                                              , public juce::ValueTree::Listener
                                              // , public juce::AudioProcessorValueTreeState::Listener
                                              , public juce::ChangeListener //监听juce::ChangeBroadcaster的广播

{
public:
    // void bindParameterListener();
    void makeVisible();
    void setUIStyle();
    void connectUIAndAudioParameter();
    void initUITriggerEvent();
    void setWindowSize();
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);

    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void topFlexBox(juce::FlexBox& flexBoxTop, std::shared_ptr<FlexBox> row1, std::shared_ptr<FlexBox> row2,
                    std::shared_ptr<FlexBox> row3, std::shared_ptr<FlexBox> row4, std::shared_ptr<FlexBox> row5,
                    std::shared_ptr<FlexBox>
                    row6, std::shared_ptr<FlexBox> row7);
    void bottomFlexBox(juce::FlexBox& bottomFlexBox, std::shared_ptr<juce::FlexBox> column1,
                       std::shared_ptr<juce::FlexBox> column2, std::shared_ptr<juce::FlexBox> column3,
                       std::shared_ptr<juce::FlexBox> column4, std::shared_ptr<juce::FlexBox> column5,
                       std::shared_ptr<juce::FlexBox> column6, std::shared_ptr<juce::FlexBox> column7,
                       std::shared_ptr<juce::FlexBox> column8, std::shared_ptr<juce::FlexBox> column9);
    void midFlexBox(juce::FlexBox& midFlexBox, std::shared_ptr<juce::FlexBox> row11,
                    std::shared_ptr<juce::FlexBox> row12,
                    std::shared_ptr<juce::FlexBox> row13, std::shared_ptr<juce::FlexBox> row14,
                    std::shared_ptr<juce::FlexBox> row15);

    void resized() override;

    //===自定义===
    // 更新波形的函数
    // void updateWaveform(const juce::AudioBuffer<float>& buffer);
    //定时回调
    // void timerCallback() override;
    void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;

    //====================自定义函数和函数重写====================
    //重载preset，所有reload preset在这里统一更新，不要在别的地方如ui event里同时更新
    void reloadPresetUI();

    //监听广播
    //reload preset时，如果数据变化也会体现在ui element event中
    //parameterChanged是音频线程，不操作UI，实时，优先级最高，changeListenerCallback是消息线程（主线程）
    //实际情况是parameterChanged会先执行，接着是changeListenerCallback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //===卷积处理：加载impulse文件数据===
    //文件选择
    void openFileChooser();
    //保存file到synth中
    void saveFileIntoSynth(const juce::File& file);
    //convolution加载sample impulse
    void loadSampleImpulseWhenSelected();

    //初始化template impulse下拉框内容
    void initTemplateImpulseComboboxNames();
    //convolution加载来自template设置的impulse
    void loadTemplateImpulseWhenSelected();
    void saveTemplateImpulseThenLoadAfterSelect(juce::String selectedId);
    //===卷积处理：加载impulse文件数据===

    void rebalanceStepHitValueDisplay();

private:
    // This reference is provided as a quick way for your editor to access the processor object that created it.
    juce::AudioVisualiserComponent visualiser = juce::AudioVisualiserComponent(0); // 波形显示组件

    //自定义控件

    //output gain
    juce::Label outputGainLabel;
    juce::Slider outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    //bpm
    juce::Slider bpmSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;

    //播放模式
    juce::Label playModeLabel;
    juce::ComboBox playModeCombobox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> playModeComboboxAttachment;

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
    juce::Slider euclidStepSlider;

    juce::Label euclidHitLabel;
    juce::Slider euclidHitSlider;

    juce::Label stochasticMaskLabel;
    juce::TextEditor stochasticMaskTextEditor;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maskOptionComboBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidStepDialAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidHitDialAttachment;

    //convolution impulse
    juce::Label impulseSwitchLabel;
    juce::ComboBox impulseSwitchComboBox; //0禁用 1使用

    juce::Label impulseTemplateLabel;
    juce::ComboBox impulseTemplateFileComboBox;

    juce::TextEditor sampleImpulsePathTextEditor; //impulse file path
    juce::TextButton selectSampleImpulseFileButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> impulseSwitchComboBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> impulseTemplateFileComboBoxAttachment;

    //文件选择
    std::unique_ptr<juce::FileChooser> fileChooser;

    AudioPluginAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
