#pragma once

//#include <juce_audio_utils/gui/juce_AudioVisualiserComponent.h>
#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
                                              // , public juce::Timer
                                              // , public juce::AudioProcessorValueTreeState::Listener
                                              // , public juce::ValueTree::Listener //监听property变化
                                              , public juce::ChangeListener //监听juce::ChangeBroadcaster的广播

{
public:
    void makeVisible();
    void setUIStyle();
    void connectUIAndAudioParameter();
    void initUITriggerEvent();
    void setWindowSize();
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);

    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void topFlexBox(juce::FlexBox& flexBoxTop, std::shared_ptr<FlexBox> trainLenFlexBox,
                    std::shared_ptr<FlexBox> trainDutyCycleFlexBox,
                    std::shared_ptr<FlexBox> trainSilenceLenFlexBox, std::shared_ptr<FlexBox> bpmFlexBox,
                    std::shared_ptr<FlexBox> playModeAndImpulseFlexBox);
    void bottomFlexBox(juce::FlexBox& bottomFlexBox, std::shared_ptr<juce::FlexBox> pulsarWaveformFlexBox,
                       std::shared_ptr<juce::FlexBox> pulsarDutyCycleClusterLenFlexBox,
                       std::shared_ptr<juce::FlexBox> pulsarDutyCycleRatioFlexBox,
                       std::shared_ptr<juce::FlexBox> ampLfoFlexBox,
                       std::shared_ptr<juce::FlexBox> formantFreqLfoFlexBox,
                       std::shared_ptr<juce::FlexBox> attackFlexBox, std::shared_ptr<juce::FlexBox> decayFlexBox,
                       std::shared_ptr<juce::FlexBox> sustainFlexBox, std::shared_ptr<juce::FlexBox> releaseFlexBox);
    void midFlexBox(juce::FlexBox& midFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox,
                    std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                    std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox,
                    std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox);

    void resized() override;

    /**
     * When the asynchronous change message is received, juce will call back this method
     * Note: During the test, it was found that the parameterChanged listening would be executed first,
     * followed by changeListenerCallback
     *
     * @param source listener
     */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //========================================自定义方法======================================
    /**
     * Update the parameters of the preset to the UI
     */
    void refreshUIFromPreset();

    //=======methods related to impulse file=======
    /**
     * open file window and select impulse file
     */
    void openFileChooser();

    /**
     * Save the impulse file to the convolution resource memory of synth
     * @param file impulse file，as juce default surpport format like .wav, .mp3
     */
    void saveFileIntoSynth(const juce::File& file);

    /**
     * When sample impulse is selected, the sample file is loaded as impulse response
     */
    void loadSampleImpulseWhenSelected();

    /**
     * init template impulse option
     */
    void initTemplateImpulseComboboxNames();

    /**
     * When template impulse is selected, the template file is loaded as impulse response
     */
    void loadTemplateImpulseWhenSelected();

    /**
     * Save template impulse to synth. If template impulse is selected,
     * it will be directly loaded as impulse response
     *
     * @param selectedId
     */
    void saveTemplateImpulseThenLoadAfterSelect(juce::String selectedId);

    void rebalanceStepHitValueDisplay();

private:
    // This reference is provided as a quick way for your editor to access the processor object that created it.
    juce::AudioVisualiserComponent visualiser = juce::AudioVisualiserComponent(0); // 波形显示组件

    //==============================自定义控件==============================
    //output gain
    juce::Label outputGainLabel;
    juce::Slider outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    //bpm
    juce::Slider bpmSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;

    //play mode
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
    juce::ComboBox impulseSwitchComboBox; //0 disable 1 enable

    juce::Label impulseTemplateLabel;
    juce::ComboBox impulseTemplateFileComboBox;

    juce::TextEditor sampleImpulsePathTextEditor; //impulse file path
    juce::TextButton selectSampleImpulseFileButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> impulseSwitchComboBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> impulseTemplateFileComboBoxAttachment;

    //open file chooser window and select file
    std::unique_ptr<juce::FileChooser> fileChooser;

    AudioPluginAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
