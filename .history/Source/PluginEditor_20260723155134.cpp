#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "include/EnvelopeCanvas.h"
#include <limits>

//===================================核心逻辑 START===========================================

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), mainPage(p, p.apvts), lfoPage(p, p.apvts), trainEnvelopePage(p, p.apvts) {
  juce::ignoreUnused(processorRef);

  // register the editor as a listener
  processorRef.addChangeListener(this);

  // init window size
  setWindowSize();

  // set ui element visible
  makeVisible();

  // ui element style
  // setUIStyle();

  // 设置attachment，保证ui element和parameter同步
  // connectUIAndAudioParameter();

  // trigger event
  initUITriggerEvent();

  // 设置初始值：上一次窗口打开的值
  setLastValueAfterCloseWindow();

  // The listening of apvts is placed at the end to ensure that the
  // modifications will not be overwritten by the previous logic. When DAW opens
  // the project, it may call setStateInformation multiple times, but
  // plugineditor has not been generated yet, resulting in the broadcast
  // changelistener listening function not being executed So the loading preset
  // tag will eventually end only after the editor (when the plugin window is
  // opened) is updated, ensuring that the UI display is up to date
  if (processorRef.isLoadingPresetFlag()) {
    changeListenerCallback(&processorRef);
  }
  showPage(Page::Main);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
  // release
  processorRef.removeChangeListener(this);
}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  // (Our component is opaque, so we must completely fill the background with a
  // solid colour)
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  // g.setColour(juce::Colours::white);
  // g.setFont(15.0f);
  // g.drawFittedText("Hello Mr. Wang! When will you return to the distant
  // planet", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::setLastValueAfterCloseWindow() {
  juce::String currentStochasticMaskStr = juce::String(processorRef.getPulsarSynthEngine().getCurrentPulsarSynth()->getStochasticMaskStr());
  if (mainPage.stochasticMaskTextEditor.getText() != currentStochasticMaskStr) {
    if (processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask) != currentStochasticMaskStr) {
      // property将会被存为state information，用来恢复参数
      processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
    }
    // 展示最新stochastic mask
    mainPage.stochasticMaskTextEditor.setText(currentStochasticMaskStr, juce::dontSendNotification);
  }
  if (!processorRef.apvts.state.getProperty(why::PropertyID::burstMask).isVoid()) {
    juce::String newText = processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString();
    if (newText != mainPage.burstMaskTextEditor.getText()) {
      mainPage.burstMaskTextEditor.setText(newText, juce::dontSendNotification);
    }
  }
}

void AudioPluginAudioProcessorEditor::showPage(Page page) {
  currentPage = page;
  mainPage.setVisible(page == Page::Main);
  lfoPage.setVisible(page == Page::LFO);
  trainEnvelopePage.setVisible(page == Page::TrainEnvelope);
  resized();
}

/**
 * set ui element display
 */
void AudioPluginAudioProcessorEditor::resized() {
  // 获取当前窗口的区域
  auto area = getLocalBounds();
  auto topBar = area.removeFromTop(40);
  mainPageButton.setBounds(topBar.removeFromLeft(80));
  trainEnvelopePageButton.setBounds(topBar.removeFromLeft(120));
  lfoPageButton.setBounds(topBar.removeFromLeft(120));

  mainPage.setBounds(area);
  lfoPage.setBounds(area);
  trainEnvelopePage.setBounds(area);
}

/**
 * Mainly handle UI updates where attachment cannot be set, such as the loading
 * logic of texteditor and impulse file
 */
void AudioPluginAudioProcessorEditor::refreshUIFromPreset() {
  if (!processorRef.isLoadingPresetFlag()) {
    return;
  }
  // Refresh the texteditor display
  // 保证展示不为空
  if (!processorRef.apvts.state.getProperty(why::PropertyID::burstMask).isVoid()) {
    juce::String burstMask = processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString();
    if (mainPage.burstMaskTextEditor.getText() != burstMask) {
      mainPage.burstMaskTextEditor.setText(burstMask, juce::dontSendNotification);
    }
  }
  if (!processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid()) {
    juce::String stochasticMaskText = processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString();
    if (mainPage.stochasticMaskTextEditor.getText() != stochasticMaskText) {
      mainPage.stochasticMaskTextEditor.setText(stochasticMaskText, juce::dontSendNotification);
    }
  }
}

/**
 * invoke listener callback
 * @param source listener
 */
void AudioPluginAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster *source) {
  // reload preset：It will execute up to here only after the plugin window is
  // opened
  if (source == &processorRef && processorRef.isLoadingPresetFlag()) {
    refreshUIFromPreset();
    processorRef.setLoadingPresetFlag(false);
    return;
  }

  // It is not triggered by reload preset but others like by parameterChanged
  if (source == &processorRef) {
    // 当train参数改变后，随机mask展示要刷新
    juce::String currentStochasticMaskStr = juce::String(processorRef.getPulsarSynthEngine().getCurrentPulsarSynth()->getStochasticMaskStr());
    if (mainPage.stochasticMaskTextEditor.getText() != currentStochasticMaskStr) {
      if (processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask) != currentStochasticMaskStr) {
        // property将会被存为state information，用来恢复参数
        processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
      }
      // 展示最新stochastic
      // mask，最好加上dontSendNotification，将不会触发TextEditor::Listener；
      mainPage.stochasticMaskTextEditor.setText(currentStochasticMaskStr, juce::dontSendNotification);
    }
  }

  // AM 包络画布数据变化时同步到 synth
  if (source == &trainEnvelopePage.ampEnvelopeCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setAmpEnvelopeData(trainEnvelopePage.ampEnvelopeCanvas.getEnvelopeData());
    });
  }

  // FM 包络画布数据变化时同步到 synth
  if (source == &trainEnvelopePage.fmEnvelopeCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setFmEnvelopeData(trainEnvelopePage.fmEnvelopeCanvas.getEnvelopeData());
    });
  }

  // duty cycle ratio 包络画布数据变化时同步到 synth
  if (source == &trainEnvelopePage.dutyCycleRatioEnvelopeCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setDutyCycleRatioEnvelopeData(trainEnvelopePage.dutyCycleRatioEnvelopeCanvas.getEnvelopeData());
    });
  }

  // duty cycle cluster 包络画布数据变化时同步到 synth
  if (source == &trainEnvelopePage.dutyCycleClusterEnvelopeCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setDutyCycleClusterEnvelopeData(trainEnvelopePage.dutyCycleClusterEnvelopeCanvas.getEnvelopeData());
    });
  }

  // Pg Waveform 包络画布数据变化时同步到 synth
  if (source == &mainPage.pgWaveformEnvelopeCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setPgWaveformEnvelopeData(mainPage.pgWaveformEnvelopeCanvas.getEnvelopeData());
    });
  }

  // FM 包络画布数据变化时同步到 synth
  if (source == &lfoPage.fmLfoCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setFmLfoData(lfoPage.fmLfoCanvas.getEnvelopeData());
    });
  }

  // AM 包络画布数据变化时同步到 synth
  if (source == &lfoPage.amLfoCanvas) {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { //
      synth->setAmLfoData(lfoPage.amLfoCanvas.getEnvelopeData());
    });
  }
}

//===================================核心逻辑 END===========================================
/**
 * set ui element visible
 */
void AudioPluginAudioProcessorEditor::makeVisible() {
  // tab page
  addAndMakeVisible(mainPage);
  addAndMakeVisible(lfoPage);
  addAndMakeVisible(trainEnvelopePage);
  addAndMakeVisible(mainPageButton);
  addAndMakeVisible(lfoPageButton);
  addAndMakeVisible(trainEnvelopePageButton);

  lfoPage.setVisible(false);
  trainEnvelopePage.setVisible(false);
}

/**
 * set ui style
 */
void AudioPluginAudioProcessorEditor::setUIStyle() {}

/**
 * Set the attachment to bind the ui element to the audio parameter, thereby
 * ensuring that the latest value of the parameter is synchronized to the ui,
 * such as automation
 */
void AudioPluginAudioProcessorEditor::connectUIAndAudioParameter() {}

/**
 * Settings of all ui element event callback methods
 */
void AudioPluginAudioProcessorEditor::initUITriggerEvent() {
  mainPage.pgWaveformEnvelopeCanvas.addChangeListener(this);
  lfoPage.fmLfoCanvas.addChangeListener(this);
  lfoPage.amLfoCanvas.addChangeListener(this);
  trainEnvelopePage.dutyCycleRatioEnvelopeCanvas.addChangeListener(this);
  trainEnvelopePage.dutyCycleClusterEnvelopeCanvas.addChangeListener(this);
  trainEnvelopePage.fmEnvelopeCanvas.addChangeListener(this);
  trainEnvelopePage.ampEnvelopeCanvas.addChangeListener(this);

  // tab page
  mainPageButton.onClick = [this] { showPage(Page::Main); };
  lfoPageButton.onClick = [this] { showPage(Page::LFO); };
  trainEnvelopePageButton.onClick = [this] { showPage(Page::TrainEnvelope); };
}

/**
 * euclid linkage setting: When the euclid step changes, the value range of
 * euclid hit is automatically adjusted
 */
void AudioPluginAudioProcessorEditor::rebalanceStepHitValueDisplay() {
  double stepValue = mainPage.euclidStepSlider.getValue();
  double hitValue = mainPage.euclidHitSlider.getValue();

  // 更新 sliderB 的最大值
  if (stepValue <= 1) {
    // juce的逻辑：range不可以设置为1，1，必须满足max>min
    // 所以step为1，hit不设为1了，直接禁用
    mainPage.euclidHitSlider.setValue(1);
    mainPage.euclidHitSlider.setEnabled(false);
  } else {
    mainPage.euclidHitSlider.setEnabled(true);
    mainPage.euclidHitSlider.setRange(mainPage.euclidHitSlider.getMinimum(), stepValue);
  }

  if (hitValue > stepValue)
    mainPage.euclidHitSlider.setValue(stepValue, juce::dontSendNotification); // 避免无限触发
}

/**
 * 设置窗口大小
 */
void AudioPluginAudioProcessorEditor::setWindowSize() {
  // Make sure that before the constructor has finished, you've set the editor's
  // size to whatever you need it to be.
  setSize(1200, 600);
  // 允许窗口被用户调整大小
  setResizable(true, true);
  // 设置最小和最大尺寸
  setResizeLimits(1024, 600, 1920, 1080);
}
