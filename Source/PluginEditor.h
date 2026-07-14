#pragma once

// #include <juce_audio_utils/gui/juce_AudioVisualiserComponent.h>
#include "PluginProcessor.h"
#include "include/EnvelopeCanvas.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
    // , public juce::Timer
    // , public juce::AudioProcessorValueTreeState::Listener
    // , public juce::ValueTree::Listener //监听property变化
    ,
                                              public juce::ChangeListener // 监听juce::ChangeBroadcaster的广播

{
public:
  void makeVisible();
  void setUIStyle();
  void connectUIAndAudioParameter();
  void initUITriggerEvent();
  void setWindowSize();
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);

  ~AudioPluginAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics &) override;

  void topFlexBox(juce::FlexBox &flexBoxTop, std::shared_ptr<juce::FlexBox> trainLenFlexBox, std::shared_ptr<juce::FlexBox> trainDutyCycleFlexBox,
                  std::shared_ptr<juce::FlexBox> trainSilenceLenFlexBox, std::shared_ptr<juce::FlexBox> bpmFlexBox);

  void midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox,
                  std::shared_ptr<juce::FlexBox> burstMaskFlexBox, std::shared_ptr<juce::FlexBox> euclidStepFlexBox,
                  std::shared_ptr<juce::FlexBox> euclidHitFlexBox, std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox,
                  std::shared_ptr<juce::FlexBox> attackFlexBox, std::shared_ptr<juce::FlexBox> decayFlexBox,
                  std::shared_ptr<juce::FlexBox> sustainFlexBox, std::shared_ptr<juce::FlexBox> releaseFlexBox);

  void resized() override;

  /**
   * When the asynchronous change message is received, juce will call back this method
   * Note: During the test, it was found that the parameterChanged listening would be executed first,
   * followed by changeListenerCallback
   *
   * @param source listener
   */
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  //========================================自定义方法======================================
  /**
   * set lasted value after close window
   */
  void setLastValueAfterCloseWindow();

  /**
   * Update the parameters of the preset to the UI
   */
  void refreshUIFromPreset();

  void rebalanceStepHitValueDisplay(); 

private:
  // This reference is provided as a quick way for your editor to access the processor object that created it.
  juce::AudioVisualiserComponent visualiser = juce::AudioVisualiserComponent(0); // 波形显示组件

  //==============================自定义控件==============================
  // output gain
  juce::Label outputGainLabel;
  juce::Slider outputGainSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

  // bpm
  juce::Slider bpmSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
 
  // train
  juce::Slider trainLenSlider;
  juce::Label trainLenLabel;

  juce::Slider trainDutyCycleLenSlider;
  juce::Label trainDutyCycleLenLabel;

  juce::Slider trainSilenceLenSlider;
  juce::Label trainSilenceLenLabel;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainLenAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainDutyCycleAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trainSilenceAttachment;

  // ========================== Pg Waveform 包络绘制组件 ==========================
  EnvelopeCanvas pgWaveformEnvelopeCanvas;
  juce::Label pgWaveformEnvelopeLabel;
  juce::TextButton pgWaveformEnvelopeClearButton{"Clear"};           // 清空（重置为 sine）
  juce::TextButton pgWaveformEnvelopeRandomButton{"Randomize"};      // 随机绘图
  juce::TextButton pgWaveformLoadFileButton{"Load File"};            // 从音频文件加载波形
  juce::Component pgWaveformEnvControlRow;                           // 开关+按钮容器
  juce::Slider pgWaveformEnvelopeScaleSlider;                        // 缩放
  juce::Label pgWaveformEnvelopeScaleLabel;

  // juce::Slider pulsarDutyCycleClusterLenSlider;
  // juce::Label pulsarDutyCycleClusterLenLabel;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarWaveformAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarDutyCycleClusterLenAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsarDutyCycleRatioAttachment;

  // ========================== AM 包络绘制组件 ==========================
  EnvelopeCanvas ampEnvelopeCanvas;
  juce::Label ampEnvelopeLabel;
  juce::Slider ampEnvelopeYMinSlider;                      // Y轴最小值
  juce::Label ampEnvelopeYMinLabel;
  juce::Slider ampEnvelopeYMaxSlider; // Y轴最大值
  juce::Label ampEnvelopeYMaxLabel;
  juce::TextButton ampEnvelopeClearButton{"Clear"};      // 清空包络
  juce::TextButton ampEnvelopeRandomButton{"Randomize"}; // 随机生成包络
  juce::TextButton ampEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component ampEnvControlRow;                      // 开关+按钮容器
  juce::Component ampEnvRangeRow;                        // Y轴范围滑块容器
  juce::Slider ampEnvelopeScaleSlider;                   // 缩放
  juce::Label ampEnvelopeScaleLabel;
  juce::Label ampLfoDepthLabel;                          // 调制深度
  juce::Slider ampLfoDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ampLfoDepthAttachment;

  // ========================== FM 包络绘制组件 ==========================
  EnvelopeCanvas fmEnvelopeCanvas;
  juce::Label fmEnvelopeLabel;
  juce::Slider fmEnvelopeYMinSlider;                      // Y轴最小值 (semitones)
  juce::Label fmEnvelopeYMinLabel;
  juce::Slider fmEnvelopeYMaxSlider; // Y轴最大值 (semitones)
  juce::Label fmEnvelopeYMaxLabel;
  juce::TextButton fmEnvelopeClearButton{"Clear"};      // 清空包络
  juce::TextButton fmEnvelopeRandomButton{"Randomize"}; // 随机生成包络
  juce::TextButton fmEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component fmEnvControlRow;                      // FM 开关+按钮容器
  juce::Component fmEnvRangeRow;                        // FM Y轴范围滑块容器
  juce::Slider fmEnvelopeScaleSlider;                   // 缩放
  juce::Label fmEnvelopeScaleLabel;
  juce::Label fmLfoDepthLabel;                          // 调制深度
  juce::Slider fmLfoDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantFreqLfoDepthAttachment;

  // ========================== pulsar duty cycle ratio 包络绘制组件  ==========================
  EnvelopeCanvas dutyCycleRatioEnvelopeCanvas;
  juce::Label dutyCycleRatioEnvelopeLabel;
  juce::Slider dutyCycleRatioEnvelopeYMinSlider;                                    // Y轴最小值
  juce::Label dutyCycleRatioEnvelopeYMinLabel;
  juce::Slider dutyCycleRatioEnvelopeYMaxSlider; // Y轴最大值
  juce::Label dutyCycleRatioEnvelopeYMaxLabel;
  juce::TextButton dutyCycleRatioEnvelopeClearButton{"Clear"};      // 清空包络
  juce::TextButton dutyCycleRatioEnvelopeRandomButton{"Randomize"}; // 随机生成包络
  juce::TextButton dutyCycleRatioEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component dutyCycleRatioEnvControlRow;                      //   开关+按钮容器
  juce::Component dutyCycleRatioEnvRangeRow;                        //   Y轴范围滑块容器
  juce::Slider dutyCycleRatioEnvelopeScaleSlider;                   // 缩放
  juce::Label dutyCycleRatioEnvelopeScaleLabel;
  juce::Label dutyCycleRatioDepthLabel;                             // 调制深度
  juce::Slider dutyCycleRatioDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dutyCycleRatioDepthAttachment;

  // ========================== pulsar duty cycle cluster 包络绘制组件  ==========================
  EnvelopeCanvas dutyCycleClusterEnvelopeCanvas;
  juce::Label dutyCycleClusterEnvelopeLabel;
  juce::Slider dutyCycleClusterEnvelopeYMinSlider;                                      // Y轴最小值
  juce::Label dutyCycleClusterEnvelopeYMinLabel;
  juce::Slider dutyCycleClusterEnvelopeYMaxSlider; // Y轴最大值 (semitones)
  juce::Label dutyCycleClusterEnvelopeYMaxLabel;
  juce::TextButton dutyCycleClusterEnvelopeClearButton{"Clear"};      // 清空包络
  juce::TextButton dutyCycleClusterEnvelopeRandomButton{"Randomize"}; // 随机生成包络
  juce::TextButton dutyCycleClusterEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component dutyCycleClusterEnvControlRow;                      //   开关+按钮容器
  juce::Component dutyCycleClusterEnvRangeRow;                        //   Y轴范围滑块容器
  juce::Slider dutyCycleClusterEnvelopeScaleSlider;                   // 缩放
  juce::Label dutyCycleClusterEnvelopeScaleLabel;
  juce::Label dutyCycleClusterDepthLabel;                             // 调制深度
  juce::Slider dutyCycleClusterDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dutyCycleClusterDepthAttachment;

  // envelope
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

  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> maskOptionComboBoxAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidStepDialAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> euclidHitDialAttachment;

  AudioPluginAudioProcessor &processorRef;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
