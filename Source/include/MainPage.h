
#pragma once
#include "../include/EnvelopeCanvas.h"
#include "PluginProcessor.h"
#include <array>
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>
class MainPage : public juce::Component {
public:
  MainPage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &apvts);
  void resized() override;
  void setUIStyle();
  void initUITriggerEvent();
  void makeVisible();
  void connectUIAndAudioParameter();
  void topFlexBox(juce::FlexBox &flexBoxTop, std::shared_ptr<juce::FlexBox> trainLenFlexBox, std::shared_ptr<juce::FlexBox> trainDutyCycleFlexBox,
                  std::shared_ptr<juce::FlexBox> trainSilenceLenFlexBox, std::shared_ptr<juce::FlexBox> bpmFlexBox);

  void midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox, std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                  std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox, std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox,
                  std::shared_ptr<juce::FlexBox> attackFlexBox, std::shared_ptr<juce::FlexBox> decayFlexBox, std::shared_ptr<juce::FlexBox> sustainFlexBox,
                  std::shared_ptr<juce::FlexBox> releaseFlexBox);

  void rebalanceStepHitValueDisplay();
  //==============================自定义控件==============================
  // output gain
  juce::Label outputGainLabel;
  juce::Slider outputGainSlider;
    
  // bpm
  juce::Slider bpmSlider;

  // train
  juce::Slider trainLenSlider;
  juce::Label trainLenLabel;

  juce::Slider trainDutyCycleLenSlider;
  juce::Label trainDutyCycleLenLabel;

  juce::Slider trainSilenceLenSlider;
  juce::Label trainSilenceLenLabel;

  // ========================== Pg Waveform 包络绘制组件 ==========================
  EnvelopeCanvas pgWaveformEnvelopeCanvas;
  juce::Label pgWaveformEnvelopeLabel;
  juce::TextButton pgWaveformEnvelopeClearButton{"Clear"};   // 清空（重置为 sine）
  juce::TextButton pgWaveformEnvelopeRandomButton{"Random"}; // 随机绘图
  juce::TextButton pgWaveformLoadFileButton{"Load File"};    // 从音频文件加载波形
  juce::Component pgWaveformEnvControlRow;                   // 开关+按钮容器
  juce::Slider pgWaveformEnvelopeScaleSlider;                // 缩放
  juce::Label pgWaveformEnvelopeScaleLabel; 

  // envelope
  juce::Slider attackSlider;
  juce::Label attackLabel;

  juce::Slider decaySlider;
  juce::Label decayLabel;

  juce::Slider sustainSlider;
  juce::Label sustainLabel;

  juce::Slider releaseSlider;
  juce::Label releaseLabel;

  // masking
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

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainLenAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainDutyCycleAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainSilenceAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maskOptionComboBoxAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidStepDialAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidHitDialAttachment;

  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &apvts;
};