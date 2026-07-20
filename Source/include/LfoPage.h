//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "../include/EnvelopeCanvas.h"
#include "PluginProcessor.h"
#include <array>
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>
class LfoPage : public juce::Component {
public:
  LfoPage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &apvts);

  void resized() override;
  void setUIStyle();
  void initUITriggerEvent();
  void makeVisible();
  void connectUIAndAudioParameter();
  // ========================== FM lfo 绘制组件 ==========================
  EnvelopeCanvas fmLfoCanvas;
  juce::Label fmLfoLabel;
  juce::Slider fmLfoYMinSlider;
  juce::Label fmLfoYMinLabel;
  juce::Slider fmLfoYMaxSlider;
  juce::Label fmLfoYMaxLabel;
  juce::TextButton fmLfoClearButton{"Clear"};        // 清空包络
  juce::TextButton fmLfoRandomButton{"Random"};      // 随机生成包络
  juce::TextButton fmLfoLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component fmLfoControlRow;                   // FM 开关+按钮容器
  juce::Component fmLfoRangeRow;                     // FM Y轴范围滑块容器
  juce::Slider fmLfoScaleSlider;                     // 缩放
  juce::Label fmLfoScaleLabel;
  juce::Label fmLfoDepthLabel; // 调制深度
  juce::Slider fmLfoDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fmLfoDepthAttachment;
  // ========================== AM lfo 绘制组件 ==========================
  EnvelopeCanvas amLfoCanvas;
  juce::Label amLfoLabel;
  juce::Slider amLfoYMinSlider;
  juce::Label amLfoYMinLabel;
  juce::Slider amLfoYMaxSlider;
  juce::Label amLfoYMaxLabel;
  juce::TextButton amLfoClearButton{"Clear"};        // 清空包络
  juce::TextButton amLfoRandomButton{"Random"};      // 随机生成包络
  juce::TextButton amLfoLoadFileButton{"Load File"}; // 从音频文件加载波形
  juce::Component amLfoControlRow;                   // am 开关+按钮容器
  juce::Component amLfoRangeRow;                     // am Y轴范围滑块容器
  juce::Slider amLfoScaleSlider;                     // 缩放
  juce::Label amLfoScaleLabel;
  juce::Label amLfoDepthLabel; // 调制深度
  juce::Slider amLfoDepthSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amLfoDepthAttachment;
  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &apvts;
};