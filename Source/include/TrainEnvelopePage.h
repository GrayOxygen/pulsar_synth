#pragma once
#include "../include/EnvelopeCanvas.h"
#include "PluginProcessor.h"
#include <array>
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>
class TrainEnvelopePage : public juce::Component {
public:
  TrainEnvelopePage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &apvts);

  void resized() override;
  void setUIStyle();
  void initUITriggerEvent();
  void makeVisible();
  void connectUIAndAudioParameter();
  //==============================自定义控件==============================

  // ========================== AM 包络绘制组件 ==========================
  EnvelopeCanvas ampEnvelopeCanvas;
  juce::Label ampEnvelopeLabel;
  juce::Slider ampEnvelopeYMinSlider; // Y轴最小值
  juce::Label ampEnvelopeYMinLabel;
  juce::Slider ampEnvelopeYMaxSlider; // Y轴最大值
  juce::Label ampEnvelopeYMaxLabel;
  juce::TextButton ampEnvelopeClearButton{"Clear"};        // 清空包络
  juce::TextButton ampEnvelopeRandomButton{"Random"};      // 随机生成包络
  juce::TextButton ampEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component ampEnvControlRow;                        // 开关+按钮容器
  juce::Component ampEnvRangeRow;                          // Y轴范围滑块容器
  juce::Slider ampEnvelopeScaleSlider;                     // 缩放
  juce::Label ampEnvelopeScaleLabel;
  juce::Label amEnvelopeDepthLabel; // 调制深度
  juce::Slider amEnvelopeDepthSlider;

  // ========================== FM 包络绘制组件 ==========================
  EnvelopeCanvas fmEnvelopeCanvas;
  juce::Label fmEnvelopeLabel;
  juce::Slider fmEnvelopeYMinSlider; // Y轴最小值 (semitones)
  juce::Label fmEnvelopeYMinLabel;
  juce::Slider fmEnvelopeYMaxSlider; // Y轴最大值 (semitones)
  juce::Label fmEnvelopeYMaxLabel;
  juce::TextButton fmEnvelopeClearButton{"Clear"};        // 清空包络
  juce::TextButton fmEnvelopeRandomButton{"Random"};      // 随机生成包络
  juce::TextButton fmEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component fmEnvelopeControlRow;                   // FM 开关+按钮容器
  juce::Component fmEnvelopeRangeRow;                     // FM Y轴范围滑块容器
  juce::Slider fmEnvelopeScaleSlider;                     // 缩放
  juce::Label fmEnvelopeScaleLabel;
  juce::Label fmEnvelopeDepthLabel; // 调制深度
  juce::Slider fmEnvelopeDepthSlider;

  // ========================== pulsar duty cycle ratio 包络绘制组件  ==========================
  EnvelopeCanvas dutyCycleRatioEnvelopeCanvas;
  juce::Label dutyCycleRatioEnvelopeLabel;
  juce::Slider dutyCycleRatioEnvelopeYMinSlider; // Y轴最小值
  juce::Label dutyCycleRatioEnvelopeYMinLabel;
  juce::Slider dutyCycleRatioEnvelopeYMaxSlider; // Y轴最大值
  juce::Label dutyCycleRatioEnvelopeYMaxLabel;
  juce::TextButton dutyCycleRatioEnvelopeClearButton{"Clear"};        // 清空包络
  juce::TextButton dutyCycleRatioEnvelopeRandomButton{"Randomize"};   // 随机生成包络
  juce::TextButton dutyCycleRatioEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component dutyCycleRatioEnvControlRow;                        //   开关+按钮容器
  juce::Component dutyCycleRatioEnvRangeRow;                          //   Y轴范围滑块容器
  juce::Slider dutyCycleRatioEnvelopeScaleSlider;                     // 缩放
  juce::Label dutyCycleRatioEnvelopeScaleLabel;
  juce::Label dutyCycleRatioDepthLabel; // 调制深度
  juce::Slider dutyCycleRatioDepthSlider;

  // ========================== pulsar duty cycle cluster 包络绘制组件  ==========================
  EnvelopeCanvas dutyCycleClusterEnvelopeCanvas;
  juce::Label dutyCycleClusterEnvelopeLabel;
  juce::Slider dutyCycleClusterEnvelopeYMinSlider; // Y轴最小值
  juce::Label dutyCycleClusterEnvelopeYMinLabel;
  juce::Slider dutyCycleClusterEnvelopeYMaxSlider; // Y轴最大值 (semitones)
  juce::Label dutyCycleClusterEnvelopeYMaxLabel;
  juce::TextButton dutyCycleClusterEnvelopeClearButton{"Clear"};        // 清空包络
  juce::TextButton dutyCycleClusterEnvelopeRandomButton{"Randomize"};   // 随机生成包络
  juce::TextButton dutyCycleClusterEnvelopeLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component dutyCycleClusterEnvControlRow;                        //   开关+按钮容器
  juce::Component dutyCycleClusterEnvRangeRow;                          //   Y轴范围滑块容器
  juce::Slider dutyCycleClusterEnvelopeScaleSlider;                     // 缩放
  juce::Label dutyCycleClusterEnvelopeScaleLabel;
  juce::Label dutyCycleClusterDepthLabel; // 调制深度
  juce::Slider dutyCycleClusterDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amEnvelopeDepthAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fmEnvelopeDepthAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dutyCycleRatioDepthAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dutyCycleClusterDepthAttachment;

  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &apvts;
};