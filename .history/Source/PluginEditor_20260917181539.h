#pragma once

// #include <juce_audio_utils/gui/juce_AudioVisualiserComponent.h>
#include "PluginProcessor.h"
#include "include/EnvelopeCanvas.h"
#include "include/LfoPage.h"
#include "include/MainPage.h"
#include "include/TrainEnvelopePage.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
    // , public juce::Timer
    // , public juce::AudioProcessorValueTreeState::Listener
    // , public juce::ValueTree::Listener //监听property变化
    ,
                                              public juce::ChangeListener // 监听juce::ChangeBroadcaster的广播

{
public:
  enum class Page { Main, LFO, TrainEnvelope };

  void makeVisible();
  void setUIStyle();
  void connectUIAndAudioParameter();
  void initUITriggerEvent();
  void setWindowSize();
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);

  ~AudioPluginAudioProcessorEditor() override;

  void showPage(Page page);

  //==============================================================================
  void paint(juce::Graphics &) override;

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

  Page currentPage = Page::Main;
  juce::TextButton mainPageButton{"Main"};
  juce::TextButton lfoPageButton{"LFO"};
  juce::TextButton trainEnvelopePageButton{"Train Envelope"};

  LfoPage lfoPage;
  MainPage mainPage;
  TrainEnvelopePage trainEnvelopePage;

  AudioPluginAudioProcessor &processorRef;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
