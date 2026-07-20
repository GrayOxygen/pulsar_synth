#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "include/BinaryResourceSingleton.h"
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
  trainEnvelopePageButton.setBounds(topBar.removeFromLeft(80));
  lfoPageButton.setBounds(topBar.removeFromLeft(80));

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

  // // AM 包络绘制事件：当包络被绘制时，同步到 synth
  // mainPage.ampEnvelopeCanvas.addChangeListener(this);

  // // Y轴范围变化时更新包络画布
  // mainPage.ampEnvelopeYMinSlider.onValueChange = [this] {
  //   float yMin = mainPage.ampEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.ampEnvelopeYMaxSlider.getValue();
  //   mainPage.ampEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.ampEnvelopeYMaxSlider.onValueChange = [this] {
  //   float yMin = mainPage.ampEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.ampEnvelopeYMaxSlider.getValue();
  //   mainPage.ampEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.ampEnvelopeScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback(
  //       [&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeScale(static_cast<float>(mainPage.ampEnvelopeScaleSlider.getValue())); });
  // };

  // // 清空包络按钮
  // mainPage.ampEnvelopeClearButton.onClick = [this] { mainPage.ampEnvelopeCanvas.clearEnvelope(1.0f); };

  // // 随机生成包络按钮
  // mainPage.ampEnvelopeRandomButton.onClick = [this] { mainPage.ampEnvelopeCanvas.randomize(); };

  // // FM 包络绘制事件：当包络被绘制时，同步到 synth
  // mainPage.fmEnvelopeCanvas.addChangeListener(this);

  // // FM Y轴范围变化时更新包络画布
  // mainPage.fmEnvelopeYMinSlider.onValueChange = [this] {
  //   float yMin = mainPage.fmEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.fmEnvelopeYMaxSlider.getValue();
  //   mainPage.fmEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.fmEnvelopeYMaxSlider.onValueChange = [this] {
  //   float yMin = mainPage.fmEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.fmEnvelopeYMaxSlider.getValue();
  //   mainPage.fmEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.fmEnvelopeScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeScale(static_cast<float>(mainPage.fmEnvelopeScaleSlider.getValue()));
  //   });
  // };

  // // 清空 FM 包络按钮
  // mainPage.fmEnvelopeClearButton.onClick = [this] { mainPage.fmEnvelopeCanvas.clearEnvelope(0.0f); };
  // mainPage.fmEnvelopeRandomButton.onClick = [this] { mainPage.fmEnvelopeCanvas.randomize(); };

  // // fm lfo
  // lfoPage.fmLfoCanvas.addChangeListener(this);

  // // FM Y轴范围变化时更新包络画布
  // lfoPage.fmLfoYMinSlider.onValueChange = [this] {
  //   float yMin = lfoPage.fmLfoYMinSlider.getValue();
  //   float yMax = lfoPage.fmLfoYMaxSlider.getValue();
  //   lfoPage.fmLfoCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoYRange(yMin, yMax); });
  // };

  // lfoPage.fmLfoYMaxSlider.onValueChange = [this] {
  //   float yMin = lfoPage.fmLfoYMinSlider.getValue();
  //   float yMax = lfoPage.fmLfoYMaxSlider.getValue();
  //   lfoPage.fmLfoCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoYRange(yMin, yMax); });
  // };

  // lfoPage.fmLfoScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoScale(static_cast<float>(lfoPage.fmLfoScaleSlider.getValue())); });
  // };

  // // 清空 FM 按钮
  // lfoPage.fmLfoClearButton.onClick = [this] { lfoPage.fmLfoCanvas.clearEnvelope(1.0f); };
  // lfoPage.fmLfoRandomButton.onClick = [this] { lfoPage.fmLfoCanvas.randomize(); };

  // // =================== duty cycle ratio 包络绘制事件：当包络被绘制时，同步到 synth ===================
  // mainPage.dutyCycleRatioEnvelopeCanvas.addChangeListener(this);

  // // Y轴范围变化时更新包络画布
  // mainPage.dutyCycleRatioEnvelopeYMinSlider.onValueChange = [this] {
  //   float yMin = mainPage.dutyCycleRatioEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.dutyCycleRatioEnvelopeYMaxSlider.getValue();
  //   mainPage.dutyCycleRatioEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.dutyCycleRatioEnvelopeYMaxSlider.onValueChange = [this] {
  //   float yMin = mainPage.dutyCycleRatioEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.dutyCycleRatioEnvelopeYMaxSlider.getValue();
  //   mainPage.dutyCycleRatioEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.dutyCycleRatioEnvelopeScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback(
  //       [&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeScale(static_cast<float>(mainPage.dutyCycleRatioEnvelopeScaleSlider.getValue())); });
  // };

  // // 清空包络按钮
  // mainPage.dutyCycleRatioEnvelopeClearButton.onClick = [this] { mainPage.dutyCycleRatioEnvelopeCanvas.clearEnvelope(0.5f); };
  // mainPage.dutyCycleRatioEnvelopeRandomButton.onClick = [this] { mainPage.dutyCycleRatioEnvelopeCanvas.randomize(); };
  // // ===============================================================================================

  // // =================== duty cycle cluster 包络绘制事件：当包络被绘制时，同步到 synth ===================
  // mainPage.dutyCycleClusterEnvelopeCanvas.addChangeListener(this);

  // // Y轴范围变化时更新包络画布
  // mainPage.dutyCycleClusterEnvelopeYMinSlider.onValueChange = [this] {
  //   float yMin = mainPage.dutyCycleClusterEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.dutyCycleClusterEnvelopeYMaxSlider.getValue();
  //   mainPage.dutyCycleClusterEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.dutyCycleClusterEnvelopeYMaxSlider.onValueChange = [this] {
  //   float yMin = mainPage.dutyCycleClusterEnvelopeYMinSlider.getValue();
  //   float yMax = mainPage.dutyCycleClusterEnvelopeYMaxSlider.getValue();
  //   mainPage.dutyCycleClusterEnvelopeCanvas.setYAxisRange(yMin, yMax);
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeYRange(yMin, yMax); });
  // };

  // mainPage.dutyCycleClusterEnvelopeScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback(
  //       [&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeScale(static_cast<float>(mainPage.dutyCycleClusterEnvelopeScaleSlider.getValue())); });
  // };

  // // 清空包络按钮
  // mainPage.dutyCycleClusterEnvelopeClearButton.onClick = [this] { mainPage.dutyCycleClusterEnvelopeCanvas.clearEnvelope(1.0f); };
  // mainPage.dutyCycleClusterEnvelopeRandomButton.onClick = [this] { mainPage.dutyCycleClusterEnvelopeCanvas.randomize(); };
  // // ===============================================================================================

  // mainPage.pgWaveformEnvelopeClearButton.onClick = [this] {
  //   std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> sineData;
  //   for (int i = 0; i < (int)sineData.size(); ++i) {
  //     float x = static_cast<float>(i) / (sineData.size() - 1);
  //     sineData[i] = std::sin(2.0f * juce::MathConstants<float>::pi * x); // [-1, 1]
  //   }
  //   mainPage.pgWaveformEnvelopeCanvas.setEnvelopeData(sineData);
  // };
  // mainPage.pgWaveformEnvelopeRandomButton.onClick = [this] { mainPage.pgWaveformEnvelopeCanvas.randomize(); };

  // mainPage.pgWaveformEnvelopeScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback(
  //       [&](std::shared_ptr<PulsarSynth> &synth) { synth->setPgWaveformEnvelopeScale(static_cast<float>(mainPage.pgWaveformEnvelopeScaleSlider.getValue())); });
  // };

  // mainPage.pgWaveformLoadFileButton.onClick = [this] {
  //   auto chooser = std::make_shared<juce::FileChooser>("Load audio file as waveform", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
  //   chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser &fc) {
  //     auto result = fc.getResult();
  //     if (!result.existsAsFile())
  //       return;

  //     juce::AudioFormatManager formatManager;
  //     formatManager.registerBasicFormats();
  //     std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(result));
  //     if (!reader)
  //       return;

  //     constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;
  //     int64 numSrcSamples = reader->lengthInSamples;
  //     if (numSrcSamples <= 0)
  //       return;

  //     int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
  //     juce::AudioBuffer<float> srcBuffer(1, readSamples);
  //     reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

  //     // 线性重采样到 targetSize 点
  //     std::array<float, targetSize> waveData;
  //     const float *src = srcBuffer.getReadPointer(0);
  //     int srcLen = srcBuffer.getNumSamples();
  //     for (int i = 0; i < targetSize; ++i) {
  //       float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
  //       int idx0 = static_cast<int>(srcIdx);
  //       int idx1 = std::min(idx0 + 1, srcLen - 1);
  //       float frac = srcIdx - idx0;
  //       waveData[i] = juce::jlimit(-1.0f, 1.0f, src[idx0] + frac * (src[idx1] - src[idx0]));
  //     }

  //     juce::MessageManager::callAsync([this, waveData] { mainPage.pgWaveformEnvelopeCanvas.setEnvelopeData(waveData); });
  //   });
  // };
  // // ===============================================================================================

  // // 通用音频文件加载到 envelope 的 lambda
  // auto loadAudioToEnvelope = [this](EnvelopeCanvas &canvas) {
  //   auto chooser = std::make_shared<juce::FileChooser>("Load audio file as envelope", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
  //   chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser, &canvas](const juce::FileChooser &fc) {
  //     auto result = fc.getResult();
  //     if (!result.existsAsFile())
  //       return;

  //     juce::AudioFormatManager formatManager;
  //     formatManager.registerBasicFormats();
  //     std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(result));
  //     if (!reader)
  //       return;

  //     constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;
  //     int64 numSrcSamples = reader->lengthInSamples;
  //     if (numSrcSamples <= 0)
  //       return;

  //     int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
  //     juce::AudioBuffer<float> srcBuffer(1, readSamples);
  //     reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

  //     std::array<float, targetSize> waveData;
  //     const float *src = srcBuffer.getReadPointer(0);
  //     int srcLen = srcBuffer.getNumSamples();
  //     auto range = canvas.getYAxisRange();
  //     for (int i = 0; i < targetSize; ++i) {
  //       float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
  //       int idx0 = static_cast<int>(srcIdx);
  //       int idx1 = std::min(idx0 + 1, srcLen - 1);
  //       float frac = srcIdx - idx0;
  //       float sample = src[idx0] + frac * (src[idx1] - src[idx0]);
  //       // 将 [-1, 1] 映射到 canvas 当前 Y 轴范围
  //       float mapped = range.first + (sample + 1.0f) * 0.5f * (range.second - range.first);
  //       waveData[i] = juce::jlimit(range.first, range.second, mapped);
  //     }

  //     juce::MessageManager::callAsync([&canvas, waveData] { canvas.setEnvelopeData(waveData); });
  //   });
  // };

  // mainPage.ampEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(mainPage.ampEnvelopeCanvas); };
  // mainPage.fmEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(mainPage.fmEnvelopeCanvas); };
  // mainPage.dutyCycleRatioEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(mainPage.dutyCycleRatioEnvelopeCanvas); };
  // mainPage.dutyCycleClusterEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(mainPage.dutyCycleClusterEnvelopeCanvas); };
  // lfoPage.fmLfoLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(lfoPage.fmLfoCanvas); };

  // // ===============================================================================================

  // // burst mask text editor回车，没有attachment，需要手动更新synth状态
  // mainPage.burstMaskTextEditor.onReturnKey = [&] {
  //   juce::String currentBurstMaskText = mainPage.burstMaskTextEditor.getText(); // 获取编辑框中的文本

  //   // 刷新synth的burst mask标记
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->refreshBurstMask(currentBurstMaskText); });

  //   // 保存到property，在load
  //   // preset时，可从parameterChanged监听中获得property值，从而恢复状态
  //   if (processorRef.apvts.state.getProperty(why::PropertyID::burstMask) != currentBurstMaskText) {
  //     processorRef.apvts.state.setProperty(why::PropertyID::burstMask, currentBurstMaskText, nullptr);
  //   }

  //   // color effect
  //   juce::Colour originalColour = mainPage.burstMaskTextEditor.findColour(juce::TextEditor::backgroundColourId);
  //   // Temporary highlighting
  //   mainPage.burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::mediumaquamarine);
  //   mainPage.burstMaskTextEditor.repaint();
  //   // recovery
  //   juce::Timer::callAfterDelay(300, [&, originalColour]() {
  //     mainPage.burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, originalColour);
  //     mainPage.burstMaskTextEditor.repaint();
  //   });
  // };

  // // euclid联动设置：始终保持hit<=step
  // mainPage.euclidStepSlider.onValueChange = [&] { rebalanceStepHitValueDisplay(); };
  // mainPage.euclidHitSlider.onValueChange = [&] { rebalanceStepHitValueDisplay(); };

  // // 强制更新synth依赖的bpm
  // mainPage.bpmSlider.onValueChange = [&] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->forceRefreshBpmAndRebuildTrain(mainPage.bpmSlider.getValue()); });
  // };

  // mainPage.maskOptionComboBox.onChange = [&] {
  //   // 只在选中stochastic mask时才展示生成的随机mask
  //   if (mainPage.maskOptionComboBox.getSelectedItemIndex() == static_cast<int>(why::MaskOptionEnum::StochasticMask)) {
  //     // stochastic mask默认采用第一个voice的
  //     std::string maskStrStd = processorRef.getPulsarSynthEngine().getCurrentPulsarSynth()->getStochasticMaskStr();
  //     juce::String newText = juce::String(maskStrStd);

  //     if (mainPage.stochasticMaskTextEditor.getText() != newText) {
  //       mainPage.stochasticMaskTextEditor.setText(newText, juce::dontSendNotification);
  //     }
  //   }
  // };

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
