
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
                  std::shared_ptr<juce::FlexBox> trainSilenceLenFlexBox, std::shared_ptr<juce::FlexBox> outputGainFlexBox);

  void midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox, std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                  std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox, std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox,
                  std::shared_ptr<juce::FlexBox> adsrFlexBox);

  void rebalanceStepHitValueDisplay();
  void applyBpmYRange();
  //==============================自定义控件==============================
  // output gain
  juce::Label outputGainLabel;
  juce::Slider outputGainSlider;

  // ========================== BPM 包络绘制组件（latch方式推进） ==========================
  EnvelopeCanvas bpmEnvelopeCanvas;
  juce::Label bpmEnvelopeLabel;
  juce::TextButton bpmEnvelopeClearButton{"Clear"};   // 重置为默认bpm
  juce::TextButton bpmEnvelopeRandomButton{"Random"}; // 随机生成包络
  juce::TextButton bpmLoadFileButton{"Load File"};    // 从音频文件加载bpm包络
  juce::Component bpmEnvControlRow;                   // 按钮容器
  // Y轴区间控制：决定画布显示范围以及bpm包络的min/max限幅
  juce::Label bpmYMinLabel;
  juce::Slider bpmYMinSlider;
  juce::Label bpmYMaxLabel;
  juce::Slider bpmYMaxSlider;

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

  // per-grain ADSR窗控件(与random mask同一行)：A/D/R为占grain寿命的比例，S为sustain电平
  juce::Label grainAdsrLabel;
  juce::Slider grainAdsrAttackSlider;
  juce::Slider grainAdsrDecaySlider;
  juce::Slider grainAdsrSustainSlider;
  juce::Slider grainAdsrReleaseSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainLenAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainDutyCycleAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainSilenceAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maskOptionComboBoxAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidStepDialAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidHitDialAttachment;

  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &apvts;
};