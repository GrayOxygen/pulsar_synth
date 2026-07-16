#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "include/BinaryResourceSingleton.h"
#include "include/EnvelopeCanvas.h"
#include <limits>

//===================================核心逻辑 START===========================================

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p) : AudioProcessorEditor(&p), processorRef(p) {
  juce::ignoreUnused(processorRef);

  // register the editor as a listener
  processorRef.addChangeListener(this);

  // init window size
  setWindowSize();

  // set ui element visible
  makeVisible();

  // ui element style
  setUIStyle();

  // 设置attachment，保证ui element和parameter同步
  connectUIAndAudioParameter();

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
  if (stochasticMaskTextEditor.getText() != currentStochasticMaskStr) {
    if (processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask) != currentStochasticMaskStr) {
      // property将会被存为state information，用来恢复参数
      processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
    }
    // 展示最新stochastic mask
    stochasticMaskTextEditor.setText(currentStochasticMaskStr, juce::dontSendNotification);
  }
  if (!processorRef.apvts.state.getProperty(why::PropertyID::burstMask).isVoid()) {
    juce::String newText = processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString();
    if (newText != burstMaskTextEditor.getText()) {
      burstMaskTextEditor.setText(newText, juce::dontSendNotification);
    }
  }
}

/**
 * set ui element display
 */
void AudioPluginAudioProcessorEditor::resized() {
  // 获取当前窗口的区域
  auto area = getLocalBounds();

  juce::FlexBox mainFlexBox;
  mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

  // top
  juce::FlexBox topFlexBox;
  std::shared_ptr<juce::FlexBox> row1 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row2 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row3 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row4 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row5 = std::make_shared<juce::FlexBox>();

  this->topFlexBox(topFlexBox, row1, row2, row3, row4);

  // midlle part
  juce::FlexBox midFlexBox;
  std::shared_ptr<juce::FlexBox> row10 = std::make_shared<juce::FlexBox>(); // trigger
  std::shared_ptr<juce::FlexBox> row11 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row12 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row13 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row14 = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> row15 = std::make_shared<juce::FlexBox>();

  auto attackFlexBox = std::make_shared<juce::FlexBox>();
  auto decayFlexBox = std::make_shared<juce::FlexBox>();
  auto sustainFlexBox = std::make_shared<juce::FlexBox>();
  auto releaseFlexBox = std::make_shared<juce::FlexBox>();

  this->midFlexBox(midFlexBox, row10, row11, row12, row13, row14, row15, attackFlexBox, decayFlexBox, sustainFlexBox, releaseFlexBox);

  // ================== AM/FM 包络行 - 在最底部单独一行，水平均分 ==================
  juce::FlexBox amFmRowFlexBox;
  amFmRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  amFmRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  amFmRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // AM 包络占据一半
  juce::FlexBox ampEnvelopeFlexBox;
  ampEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  ampEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  juce::FlexBox ampLfoDepthFlexBox;
  ampLfoDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  ampLfoDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  ampLfoDepthFlexBox.items.add(juce::FlexItem(ampLfoDepthLabel).withFlex(1).withMaxHeight(20));
  ampLfoDepthFlexBox.items.add(juce::FlexItem(ampLfoDepthSlider).withFlex(2.0));
  amFmRowFlexBox.items.add(juce::FlexItem(ampLfoDepthFlexBox).withFlex(0.3f));

  ampEnvelopeFlexBox.items.add(juce::FlexItem(ampEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  ampEnvelopeFlexBox.items.add(juce::FlexItem(ampEnvControlRow).withFlex(0.5).withMaxHeight(24));
  ampEnvelopeFlexBox.items.add(juce::FlexItem(ampEnvRangeRow).withFlex(0.5).withMaxHeight(32));
  ampEnvelopeFlexBox.items.add(juce::FlexItem(ampEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  amFmRowFlexBox.items.add(juce::FlexItem(ampEnvelopeFlexBox).withFlex(1.0f));

  // FM 包络占据一半
  juce::FlexBox formantLfoDepthFlexBox;
  formantLfoDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  formantLfoDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  formantLfoDepthFlexBox.items.add(juce::FlexItem(fmEnvelopeDepthLabel).withFlex(1).withMaxHeight(20));
  formantLfoDepthFlexBox.items.add(juce::FlexItem(fmEnvelopeDepthSlider).withFlex(2.0));
  amFmRowFlexBox.items.add(juce::FlexItem(formantLfoDepthFlexBox).withFlex(0.3f));

  juce::FlexBox fmEnvelopeFlexBox;
  fmEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  fmEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  fmEnvelopeFlexBox.items.add(juce::FlexItem(fmEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  fmEnvelopeFlexBox.items.add(juce::FlexItem(fmEnvelopeControlRow).withFlex(0.5).withMaxHeight(24));
  fmEnvelopeFlexBox.items.add(juce::FlexItem(fmEnvelopeRangeRow).withFlex(0.5).withMaxHeight(32));
  fmEnvelopeFlexBox.items.add(juce::FlexItem(fmEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  amFmRowFlexBox.items.add(juce::FlexItem(fmEnvelopeFlexBox).withFlex(1.0f));

  // ================== duty cycle ratio/pulsar cluster 包络行 - 在最底部单独一行，水平均分 ==================
  juce::FlexBox pulsarLengthRowFlexBox;
  pulsarLengthRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  pulsarLengthRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  pulsarLengthRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // duty cycle ratio 包络占据一半
  juce::FlexBox dutyCycleRatioDepthEnvelopeFlexBox;
  dutyCycleRatioDepthEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleRatioDepthEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleRatioDepthEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioDepthLabel).withFlex(1).withMaxHeight(20));
  dutyCycleRatioDepthEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioDepthSlider).withFlex(2.0));
  pulsarLengthRowFlexBox.items.add(juce::FlexItem(dutyCycleRatioDepthEnvelopeFlexBox).withFlex(0.3f));

  juce::FlexBox dutyCycleRatioEnvelopeFlexBox;
  dutyCycleRatioEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleRatioEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleRatioEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  dutyCycleRatioEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioEnvControlRow).withFlex(0.5).withMaxHeight(24));
  dutyCycleRatioEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioEnvRangeRow).withFlex(0.5).withMaxHeight(32));
  dutyCycleRatioEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleRatioEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  pulsarLengthRowFlexBox.items.add(juce::FlexItem(dutyCycleRatioEnvelopeFlexBox).withFlex(1.0f));

  juce::FlexBox dutyCycleClusterDepthFlexBox;
  dutyCycleClusterDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleClusterDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleClusterDepthFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthLabel).withFlex(1).withMaxHeight(20));
  dutyCycleClusterDepthFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthSlider).withFlex(2.0));
  pulsarLengthRowFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthFlexBox).withFlex(0.3f));

  juce::FlexBox dutyCycleClusterEnvelopeFlexBox;
  dutyCycleClusterEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleClusterEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvControlRow).withFlex(0.5).withMaxHeight(24));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvRangeRow).withFlex(0.5).withMaxHeight(32));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  pulsarLengthRowFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeFlexBox).withFlex(1.0f));

  // ================== fm lfo 行 ==================
  juce::FlexBox amFmLfoRowFlexBox;
  amFmLfoRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  amFmLfoRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  amFmLfoRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  juce::FlexBox fmLfoDepthFlexBox;
  fmLfoDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  fmLfoDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  fmLfoDepthFlexBox.items.add(juce::FlexItem(fmLfoDepthLabel).withFlex(1).withMaxHeight(20));
  fmLfoDepthFlexBox.items.add(juce::FlexItem(fmLfoDepthSlider).withFlex(2.0));
  amFmLfoRowFlexBox.items.add(juce::FlexItem(fmLfoDepthFlexBox).withFlex(0.3f));

  juce::FlexBox fmLfoFlexBox;
  fmLfoFlexBox.flexDirection = juce::FlexBox::Direction::column;
  fmLfoFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  fmLfoFlexBox.items.add(juce::FlexItem(fmLfoLabel).withFlex(0.5).withMaxHeight(18));
  fmLfoFlexBox.items.add(juce::FlexItem(fmLfoControlRow).withFlex(0.5).withMaxHeight(24));
  fmLfoFlexBox.items.add(juce::FlexItem(fmLfoRangeRow).withFlex(0.5).withMaxHeight(32));
  fmLfoFlexBox.items.add(juce::FlexItem(fmLfoCanvas).withFlex(2.0).withMinHeight(120));
  amFmLfoRowFlexBox.items.add(juce::FlexItem(fmLfoFlexBox).withFlex(1.0f));

  // ================== Pg Waveform 包络行 ==================
  juce::FlexBox pgWaveformRowFlexBox;
  pgWaveformRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  pgWaveformRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  pgWaveformRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  juce::FlexBox pgWaveformEnvelopeFlexBox;
  pgWaveformEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  pgWaveformEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvControlRow).withFlex(0.5).withMaxHeight(24));
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeCanvas).withFlex(2.0).withMinHeight(80));
  pgWaveformRowFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeFlexBox).withFlex(1.0f));
  
  // Overall combination, withMargin: up, right, down, left
  mainFlexBox.items.add(juce::FlexItem(topFlexBox).withFlex(0.5).withMargin({20, 20, 0, 20}));
  mainFlexBox.items.add(juce::FlexItem(midFlexBox).withFlex(0.9).withMargin({20, 20, 0, 20}));
  mainFlexBox.items.add(juce::FlexItem(amFmRowFlexBox).withFlex(2.0).withMargin({10, 10, 20, 20}));
  mainFlexBox.items.add(juce::FlexItem(amFmLfoRowFlexBox).withFlex(2.0).withMargin({10, 10, 20, 20}));
  mainFlexBox.items.add(juce::FlexItem(pulsarLengthRowFlexBox).withFlex(2.0).withMargin({10, 10, 20, 20}));
  mainFlexBox.items.add(juce::FlexItem(pgWaveformRowFlexBox).withFlex(1.5).withMargin({10, 10, 20, 20}));
  mainFlexBox.performLayout(area);

  // 为 Pg Waveform 包络容器内的控件设置布局
  auto pgWaveformControlBounds = pgWaveformEnvControlRow.getLocalBounds();
  int pgCtrlW = pgWaveformControlBounds.getWidth();
  int pgLabelW = 100;
  int pgBtnW = (pgCtrlW - pgLabelW * 2) / 3;
  pgWaveformEnvelopeClearButton.setBounds(pgWaveformControlBounds.removeFromLeft(pgBtnW));
  pgWaveformEnvelopeRandomButton.setBounds(pgWaveformControlBounds.removeFromLeft(pgBtnW));
  pgWaveformLoadFileButton.setBounds(pgWaveformControlBounds.removeFromLeft(pgBtnW));
  pgWaveformEnvelopeScaleLabel.setBounds(pgWaveformControlBounds.removeFromLeft(pgLabelW));
  pgWaveformEnvelopeScaleSlider.setBounds(pgWaveformControlBounds);

  // 为 AM 包络容器内的控件设置布局
  auto controlBounds = ampEnvControlRow.getLocalBounds();
  ampEnvelopeClearButton.setBounds(controlBounds.removeFromLeft(controlBounds.getWidth() * 0.333f));
  ampEnvelopeRandomButton.setBounds(controlBounds.removeFromLeft(controlBounds.getWidth() * 0.5f));
  ampEnvelopeLoadFileButton.setBounds(controlBounds);

  auto rangeBounds = ampEnvRangeRow.getLocalBounds();
  int labelWidth = 30;
  int sliderWidth = juce::jmax(20, (rangeBounds.getWidth() - labelWidth * 3) / 3);
  ampEnvelopeYMinLabel.setBounds(rangeBounds.removeFromLeft(labelWidth));
  ampEnvelopeYMinSlider.setBounds(rangeBounds.removeFromLeft(sliderWidth));
  ampEnvelopeYMaxLabel.setBounds(rangeBounds.removeFromLeft(labelWidth));
  ampEnvelopeYMaxSlider.setBounds(rangeBounds.removeFromLeft(sliderWidth));
  ampEnvelopeScaleLabel.setBounds(rangeBounds.removeFromLeft(labelWidth));
  ampEnvelopeScaleSlider.setBounds(rangeBounds);

  // 为 FM 包络容器内的控件设置布局
  auto fmControlBounds = fmEnvelopeControlRow.getLocalBounds();
  fmEnvelopeClearButton.setBounds(fmControlBounds.removeFromLeft(fmControlBounds.getWidth() * 0.333f));
  fmEnvelopeRandomButton.setBounds(fmControlBounds.removeFromLeft(fmControlBounds.getWidth() * 0.5f));
  fmEnvelopeLoadFileButton.setBounds(fmControlBounds);

  auto fmRangeBounds = fmEnvelopeRangeRow.getLocalBounds();
  int fmLabelWidth = 30;
  int fmSliderWidth = juce::jmax(20, (fmRangeBounds.getWidth() - fmLabelWidth * 3) / 3);
  fmEnvelopeYMinLabel.setBounds(fmRangeBounds.removeFromLeft(fmLabelWidth));
  fmEnvelopeYMinSlider.setBounds(fmRangeBounds.removeFromLeft(fmSliderWidth));
  fmEnvelopeYMaxLabel.setBounds(fmRangeBounds.removeFromLeft(fmLabelWidth));
  fmEnvelopeYMaxSlider.setBounds(fmRangeBounds.removeFromLeft(fmSliderWidth));
  fmEnvelopeScaleLabel.setBounds(fmRangeBounds.removeFromLeft(fmLabelWidth));
  fmEnvelopeScaleSlider.setBounds(fmRangeBounds);
  
  // 为 duty cycle ratio 包络容器内的控件设置布局
  auto dutyCycleRatioControlBounds = dutyCycleRatioEnvControlRow.getLocalBounds();
  dutyCycleRatioEnvelopeClearButton.setBounds(dutyCycleRatioControlBounds.removeFromLeft(dutyCycleRatioControlBounds.getWidth() * 0.333f));
  dutyCycleRatioEnvelopeRandomButton.setBounds(dutyCycleRatioControlBounds.removeFromLeft(dutyCycleRatioControlBounds.getWidth() * 0.5f));
  dutyCycleRatioEnvelopeLoadFileButton.setBounds(dutyCycleRatioControlBounds);

  auto dutyCycleRatioRangeBounds = dutyCycleRatioEnvRangeRow.getLocalBounds();
  int dutyLabelWidth = 30;
  int dutySliderWidth = juce::jmax(20, (dutyCycleRatioRangeBounds.getWidth() - dutyLabelWidth * 3) / 3);
  dutyCycleRatioEnvelopeYMinLabel.setBounds(dutyCycleRatioRangeBounds.removeFromLeft(dutyLabelWidth));
  dutyCycleRatioEnvelopeYMinSlider.setBounds(dutyCycleRatioRangeBounds.removeFromLeft(dutySliderWidth));
  dutyCycleRatioEnvelopeYMaxLabel.setBounds(dutyCycleRatioRangeBounds.removeFromLeft(dutyLabelWidth));
  dutyCycleRatioEnvelopeYMaxSlider.setBounds(dutyCycleRatioRangeBounds.removeFromLeft(dutySliderWidth));
  dutyCycleRatioEnvelopeScaleLabel.setBounds(dutyCycleRatioRangeBounds.removeFromLeft(dutyLabelWidth));
  dutyCycleRatioEnvelopeScaleSlider.setBounds(dutyCycleRatioRangeBounds);

  // 为 duty cycle cluster 包络容器内的控件设置布局
  auto dutyCycleClusterControlBounds = dutyCycleClusterEnvControlRow.getLocalBounds();
  dutyCycleClusterEnvelopeClearButton.setBounds(dutyCycleClusterControlBounds.removeFromLeft(dutyCycleClusterControlBounds.getWidth() * 0.333f));
  dutyCycleClusterEnvelopeRandomButton.setBounds(dutyCycleClusterControlBounds.removeFromLeft(dutyCycleClusterControlBounds.getWidth() * 0.5f));
  dutyCycleClusterEnvelopeLoadFileButton.setBounds(dutyCycleClusterControlBounds);

  auto dutyCycleClusterRangeBounds = dutyCycleClusterEnvRangeRow.getLocalBounds();
  int clusterLabelWidth = 30;
  int clusterSliderWidth = juce::jmax(20, (dutyCycleClusterRangeBounds.getWidth() - clusterLabelWidth * 3) / 3);
  dutyCycleClusterEnvelopeYMinLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth));
  dutyCycleClusterEnvelopeYMinSlider.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterSliderWidth));
  dutyCycleClusterEnvelopeYMaxLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth));
  dutyCycleClusterEnvelopeYMaxSlider.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterSliderWidth));
  dutyCycleClusterEnvelopeScaleLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth));
  dutyCycleClusterEnvelopeScaleSlider.setBounds(dutyCycleClusterRangeBounds);
  
  // 为 FM lfo 容器内的控件设置布局
  auto fmLfoControlBounds = fmLfoControlRow.getLocalBounds();
  fmLfoClearButton.setBounds(fmLfoControlBounds.removeFromLeft(fmLfoControlBounds.getWidth() * 0.333f));
  fmLfoRandomButton.setBounds(fmLfoControlBounds.removeFromLeft(fmLfoControlBounds.getWidth() * 0.5f));
  fmLfoLoadFileButton.setBounds(fmLfoControlBounds);
  
  auto fmLfoRangeBounds = fmLfoRangeRow.getLocalBounds();
  int fmLfoLabelWidth = 30;
  int fmLfoSliderWidth = juce::jmax(20, (fmLfoRangeBounds.getWidth() - fmLfoLabelWidth * 3) / 3);
  fmLfoYMinLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  fmLfoYMinSlider.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoSliderWidth));
  fmLfoYMaxLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  fmLfoYMaxSlider.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoSliderWidth));
  fmLfoScaleLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  fmLfoScaleSlider.setBounds(fmLfoRangeBounds); 
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
    if (burstMaskTextEditor.getText() != burstMask) {
      burstMaskTextEditor.setText(burstMask, juce::dontSendNotification);
    }
  }
  if (!processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid()) {
    juce::String stochasticMaskText = processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString();
    if (stochasticMaskTextEditor.getText() != stochasticMaskText) {
      stochasticMaskTextEditor.setText(stochasticMaskText, juce::dontSendNotification);
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
    if (stochasticMaskTextEditor.getText() != currentStochasticMaskStr) {
      if (processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask) != currentStochasticMaskStr) {
        // property将会被存为state information，用来恢复参数
        processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
      }
      // 展示最新stochastic
      // mask，最好加上dontSendNotification，将不会触发TextEditor::Listener；
      stochasticMaskTextEditor.setText(currentStochasticMaskStr, juce::dontSendNotification);
    }
  }

  // AM 包络画布数据变化时同步到 synth
  if (source == &ampEnvelopeCanvas) {
    auto envelopeData = ampEnvelopeCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeData(envelopeData); });
  }

  // FM 包络画布数据变化时同步到 synth
  if (source == &fmEnvelopeCanvas) {
    auto envelopeData = fmEnvelopeCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeData(envelopeData); });
  }

  // duty cycle ratio 包络画布数据变化时同步到 synth
  if (source == &dutyCycleRatioEnvelopeCanvas) {
    auto envelopeData = dutyCycleRatioEnvelopeCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeData(envelopeData); });
  }

  // duty cycle cluster 包络画布数据变化时同步到 synth
  if (source == &dutyCycleClusterEnvelopeCanvas) {
    auto envelopeData = dutyCycleClusterEnvelopeCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeData(envelopeData); });
  }

  // Pg Waveform 包络画布数据变化时同步到 synth
  if (source == &pgWaveformEnvelopeCanvas) {
    auto envelopeData = pgWaveformEnvelopeCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setPgWaveformEnvelopeData(envelopeData); });
  }

  // FM 包络画布数据变化时同步到 synth
  if (source == &fmLfoCanvas) {
    auto envelopeData = fmLfoCanvas.getEnvelopeData();
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoData(envelopeData); });
  }
}

//===================================核心逻辑
// END===========================================
/**
 * Head layout: Arrange each flexbox vertically (internal horizontal layout)
 * @param flexBoxTop 头部布局的flexbox
 * @param trainLenFlexBox  train len flexbox in a row
 * @param trainDutyCycleFlexBox train duty cyle flexbox in a row
 * @param trainSilenceLenFlexBox train silence length flexbox in a row
 * @param bpmFlexBox bpm flexbox in a row
 * file ui elements in a row
 */
void AudioPluginAudioProcessorEditor::topFlexBox(juce::FlexBox &flexBoxTop, std::shared_ptr<juce::FlexBox> trainLenFlexBox, std::shared_ptr<juce::FlexBox> trainDutyCycleFlexBox,
                                                 std::shared_ptr<juce::FlexBox> trainSilenceLenFlexBox, std::shared_ptr<juce::FlexBox> bpmFlexBox) {
  flexBoxTop.flexDirection = juce::FlexBox::Direction::column;
  flexBoxTop.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
  // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; //
  // 控件均匀分布
  flexBoxTop.alignContent = juce::FlexBox::AlignContent::stretch; // 在wrap换行时才有用，决定换行元素的排列
  flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

  // 每一行：Label + Slider
  trainLenFlexBox->flexDirection = juce::FlexBox::Direction::row;
  trainLenFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  trainLenFlexBox->items.add(juce::FlexItem(trainLenLabel).withFlex(1).withMaxWidth(100));
  // margin：上右下左
  trainLenFlexBox->items.add(juce::FlexItem(trainLenSlider).withFlex(2.5));
  flexBoxTop.items.add(juce::FlexItem(*trainLenFlexBox).withFlex(1.0f));

  trainDutyCycleFlexBox->flexDirection = juce::FlexBox::Direction::row;
  trainDutyCycleFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  trainDutyCycleFlexBox->items.add(juce::FlexItem(trainDutyCycleLenLabel).withFlex(1).withMaxWidth(100));
  trainDutyCycleFlexBox->items.add(juce::FlexItem(trainDutyCycleLenSlider).withFlex(2.5));
  flexBoxTop.items.add(juce::FlexItem(*trainDutyCycleFlexBox).withFlex(1.0f));

  trainSilenceLenFlexBox->flexDirection = juce::FlexBox::Direction::row;
  trainSilenceLenFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  trainSilenceLenFlexBox->items.add(juce::FlexItem(trainSilenceLenLabel).withFlex(1).withMaxWidth(100));
  trainSilenceLenFlexBox->items.add(juce::FlexItem(trainSilenceLenSlider).withFlex(2.5));
  flexBoxTop.items.add(juce::FlexItem(*trainSilenceLenFlexBox).withFlex(1.0f));

  bpmFlexBox->flexDirection = juce::FlexBox::Direction::row;
  bpmFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  bpmFlexBox->items.add(juce::FlexItem(outputGainLabel).withFlex(1).withMaxWidth(100));
  bpmFlexBox->items.add(juce::FlexItem(outputGainSlider).withFlex(2.5));
  bpmFlexBox->items.add(juce::FlexItem().withFlex(0.1));
  bpmFlexBox->items.add(juce::FlexItem(bpmSlider).withFlex(1));
  flexBoxTop.items.add(juce::FlexItem(*bpmFlexBox).withFlex(1.0f));
}

/**
 * Central flexbox layout: Arrange each flexbox horizontally (internal
 * horizontal layout)
 * @param midFlexBox 中部布局的flexbox
 * @param triggerFlexBox trigger in a row
 * @param maskOptionFlexBox mask option in a row
 * @param burstMaskFlexBox burst mask in a row
 * @param euclidStepFlexBox euclid step in a row
 * @param euclidHitFlexBox euclid hit in a row
 * @param stochasticMaskFlexBox stochastic mask in a row
 */
void AudioPluginAudioProcessorEditor::midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox,
                                                 std::shared_ptr<juce::FlexBox> burstMaskFlexBox, std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox,
                                                 std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox, std::shared_ptr<juce::FlexBox> attackFlexBox, std::shared_ptr<juce::FlexBox> decayFlexBox,
                                                 std::shared_ptr<juce::FlexBox> sustainFlexBox, std::shared_ptr<juce::FlexBox> releaseFlexBox) {
  midFlexBox.flexDirection = juce::FlexBox::Direction::row; // 水平排列

  triggerFlexBox->flexDirection = juce::FlexBox::Direction::column;
  triggerFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  // midFlexBox.items.add(juce::FlexItem(*triggerFlexBox).withFlex(1.0f));

  maskOptionFlexBox->flexDirection = juce::FlexBox::Direction::row;
  maskOptionFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  maskOptionFlexBox->items.add(juce::FlexItem(maskComboBoxLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  maskOptionFlexBox->items.add(juce::FlexItem(maskOptionComboBox).withFlex(1.0).withMaxHeight(20));
  midFlexBox.items.add(juce::FlexItem(*maskOptionFlexBox).withFlex(1.0f));

  burstMaskFlexBox->flexDirection = juce::FlexBox::Direction::row;
  burstMaskFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  burstMaskFlexBox->items.add(juce::FlexItem(burstMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  burstMaskFlexBox->items.add(juce::FlexItem(burstMaskTextEditor).withFlex(1.0).withMinWidth(100).withMaxWidth(250).withMaxHeight(150));
  midFlexBox.items.add(juce::FlexItem(*burstMaskFlexBox).withFlex(1.5f));

  euclidStepFlexBox->flexDirection = juce::FlexBox::Direction::row;
  // flexBoxTop->justifyContent = juce::FlexBox::JustifyContent::spaceAround; //
  // 控件均匀分布
  euclidStepFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  euclidStepFlexBox->items.add(juce::FlexItem(euclidStepLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  euclidStepFlexBox->items.add(juce::FlexItem(euclidStepSlider).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
  midFlexBox.items.add(juce::FlexItem(*euclidStepFlexBox).withFlex(0.9f));

  euclidHitFlexBox->flexDirection = juce::FlexBox::Direction::row;
  euclidHitFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  euclidHitFlexBox->items.add(juce::FlexItem(euclidHitLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  euclidHitFlexBox->items.add(juce::FlexItem(euclidHitSlider).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
  midFlexBox.items.add(juce::FlexItem(*euclidHitFlexBox).withFlex(0.9f));

  stochasticMaskFlexBox->flexDirection = juce::FlexBox::Direction::row;
  stochasticMaskFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  stochasticMaskFlexBox->items.add(juce::FlexItem(stochasticMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  stochasticMaskFlexBox->items.add(juce::FlexItem(stochasticMaskTextEditor).withFlex(1.0).withMaxWidth(250).withMaxHeight(150));
  midFlexBox.items.add(juce::FlexItem(*stochasticMaskFlexBox).withFlex(1.5f));

  // ADSR
  attackFlexBox->flexDirection = juce::FlexBox::Direction::column;
  attackFlexBox->items.add(juce::FlexItem(attackLabel).withFlex(1).withMaxWidth(80).withMaxHeight(20));
  attackFlexBox->items.add(juce::FlexItem(attackSlider).withFlex(2.0));
  midFlexBox.items.add(juce::FlexItem(*attackFlexBox).withFlex(0.8f));

  decayFlexBox->flexDirection = juce::FlexBox::Direction::column;
  decayFlexBox->items.add(juce::FlexItem(decayLabel).withFlex(1).withMaxWidth(80).withMaxHeight(20));
  decayFlexBox->items.add(juce::FlexItem(decaySlider).withFlex(2.0));
  midFlexBox.items.add(juce::FlexItem(*decayFlexBox).withFlex(0.8f));

  sustainFlexBox->flexDirection = juce::FlexBox::Direction::column;
  sustainFlexBox->items.add(juce::FlexItem(sustainLabel).withFlex(1).withMaxWidth(80).withMaxHeight(20));
  sustainFlexBox->items.add(juce::FlexItem(sustainSlider).withFlex(2.0));
  midFlexBox.items.add(juce::FlexItem(*sustainFlexBox).withFlex(0.8f));

  releaseFlexBox->flexDirection = juce::FlexBox::Direction::column;
  releaseFlexBox->items.add(juce::FlexItem(releaseLabel).withFlex(1).withMaxWidth(80).withMaxHeight(20));
  releaseFlexBox->items.add(juce::FlexItem(releaseSlider).withFlex(2.0));
  midFlexBox.items.add(juce::FlexItem(*releaseFlexBox).withFlex(0.8f));

  // flexBoxMiddle.performLayout(area.removeFromTop(150));
  //
  // area.removeFromTop(20); // 插入 20px 空白
}

/**
 * set ui element visible
 */
void AudioPluginAudioProcessorEditor::makeVisible() {
  // output gain
  addAndMakeVisible(outputGainLabel);
  addAndMakeVisible(outputGainSlider);

  // bpm
  addAndMakeVisible(bpmSlider);

  // train
  addAndMakeVisible(trainLenSlider);
  addAndMakeVisible(trainDutyCycleLenSlider);
  addAndMakeVisible(trainSilenceLenSlider);

  addAndMakeVisible(trainLenLabel);
  addAndMakeVisible(trainDutyCycleLenLabel);
  addAndMakeVisible(trainSilenceLenLabel);

  // Pg Waveform 包络控件
  addAndMakeVisible(pgWaveformEnvelopeCanvas);
  addAndMakeVisible(pgWaveformEnvelopeLabel);
  // addAndMakeVisible(pgWaveformEnvelopeToggle);
  addAndMakeVisible(pgWaveformEnvelopeClearButton);
  addAndMakeVisible(pgWaveformEnvelopeRandomButton);
  addAndMakeVisible(pgWaveformEnvControlRow);
  // pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformEnvelopeToggle);
  pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformEnvelopeClearButton);
  pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformEnvelopeRandomButton);
  pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformLoadFileButton);
  pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformEnvelopeScaleLabel);
  pgWaveformEnvControlRow.addAndMakeVisible(pgWaveformEnvelopeScaleSlider);

  // AM 包络控件
  addAndMakeVisible(ampEnvelopeCanvas);
  addAndMakeVisible(ampEnvelopeLabel);
  addAndMakeVisible(ampEnvelopeYMinSlider);
  addAndMakeVisible(ampEnvelopeYMinLabel);
  addAndMakeVisible(ampEnvelopeYMaxSlider);
  addAndMakeVisible(ampEnvelopeYMaxLabel);
  addAndMakeVisible(ampLfoDepthSlider);
  addAndMakeVisible(ampLfoDepthLabel);
  addAndMakeVisible(ampEnvelopeClearButton);
  addAndMakeVisible(ampEnvelopeRandomButton);
  addAndMakeVisible(ampEnvelopeLoadFileButton);
  addAndMakeVisible(ampEnvControlRow);
  addAndMakeVisible(ampEnvRangeRow);
  // 将子控件添加到容器中
  ampEnvControlRow.addAndMakeVisible(ampEnvelopeClearButton);
  ampEnvControlRow.addAndMakeVisible(ampEnvelopeRandomButton);
  ampEnvControlRow.addAndMakeVisible(ampEnvelopeLoadFileButton);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeYMinSlider);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeYMinLabel);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeYMaxSlider);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeYMaxLabel);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeScaleSlider);
  ampEnvRangeRow.addAndMakeVisible(ampEnvelopeScaleLabel);

  // FM 包络控件
  addAndMakeVisible(fmEnvelopeCanvas);
  addAndMakeVisible(fmEnvelopeLabel);
  addAndMakeVisible(fmEnvelopeYMinSlider);
  addAndMakeVisible(fmEnvelopeYMinLabel);
  addAndMakeVisible(fmEnvelopeYMaxSlider);
  addAndMakeVisible(fmEnvelopeYMaxLabel);
  addAndMakeVisible(fmEnvelopeDepthSlider);
  addAndMakeVisible(fmEnvelopeDepthLabel);
  addAndMakeVisible(fmEnvelopeClearButton);
  addAndMakeVisible(fmEnvelopeRandomButton);
  addAndMakeVisible(fmEnvelopeLoadFileButton);
  addAndMakeVisible(fmEnvelopeControlRow);
  addAndMakeVisible(fmEnvelopeRangeRow);

  // 将子控件添加到 FM 容器中
  fmEnvelopeControlRow.addAndMakeVisible(fmEnvelopeClearButton);
  fmEnvelopeControlRow.addAndMakeVisible(fmEnvelopeRandomButton);
  fmEnvelopeControlRow.addAndMakeVisible(fmEnvelopeLoadFileButton);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeYMinSlider);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeYMinLabel);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeYMaxSlider);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeYMaxLabel);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeScaleSlider);
  fmEnvelopeRangeRow.addAndMakeVisible(fmEnvelopeScaleLabel); 

  // FM lfo 控件
  addAndMakeVisible(fmLfoCanvas);
  addAndMakeVisible(fmLfoLabel);
  addAndMakeVisible(fmLfoYMinSlider);
  addAndMakeVisible(fmLfoYMinLabel);
  addAndMakeVisible(fmLfoYMaxSlider);
  addAndMakeVisible(fmLfoYMaxLabel);
  addAndMakeVisible(fmLfoDepthSlider);
  addAndMakeVisible(fmLfoDepthLabel);
  addAndMakeVisible(fmLfoClearButton);
  addAndMakeVisible(fmLfoRandomButton);
  addAndMakeVisible(fmLfoLoadFileButton);
  addAndMakeVisible(fmLfoControlRow);
  addAndMakeVisible(fmLfoRangeRow);

  // 将子控件添加到 FM lfo 容器中
  fmLfoControlRow.addAndMakeVisible(fmLfoClearButton);
  fmLfoControlRow.addAndMakeVisible(fmLfoRandomButton);
  fmLfoControlRow.addAndMakeVisible(fmLfoLoadFileButton);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMinSlider);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMinLabel);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMaxSlider);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMaxLabel);
  fmLfoRangeRow.addAndMakeVisible(fmLfoScaleSlider);
  fmLfoRangeRow.addAndMakeVisible(fmLfoScaleLabel);

  // dutyCycleRatio 包络控件
  addAndMakeVisible(dutyCycleRatioEnvelopeCanvas);
  addAndMakeVisible(dutyCycleRatioEnvelopeLabel);
  addAndMakeVisible(dutyCycleRatioEnvelopeYMinSlider);
  addAndMakeVisible(dutyCycleRatioEnvelopeYMinLabel);
  addAndMakeVisible(dutyCycleRatioEnvelopeYMaxSlider);
  addAndMakeVisible(dutyCycleRatioEnvelopeYMaxLabel);
  addAndMakeVisible(dutyCycleRatioDepthSlider);
  addAndMakeVisible(dutyCycleRatioDepthLabel);
  addAndMakeVisible(dutyCycleRatioEnvelopeClearButton);
  addAndMakeVisible(dutyCycleRatioEnvelopeRandomButton);
  addAndMakeVisible(dutyCycleRatioEnvelopeLoadFileButton);
  addAndMakeVisible(dutyCycleRatioEnvControlRow);
  addAndMakeVisible(dutyCycleRatioEnvRangeRow);
  // 将子控件添加到 dutyCycleRatio 容器中
  dutyCycleRatioEnvControlRow.addAndMakeVisible(dutyCycleRatioEnvelopeClearButton);
  dutyCycleRatioEnvControlRow.addAndMakeVisible(dutyCycleRatioEnvelopeRandomButton);
  dutyCycleRatioEnvControlRow.addAndMakeVisible(dutyCycleRatioEnvelopeLoadFileButton);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeYMinSlider);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeYMinLabel);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeYMaxSlider);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeYMaxLabel);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeScaleSlider);
  dutyCycleRatioEnvRangeRow.addAndMakeVisible(dutyCycleRatioEnvelopeScaleLabel);

  // dutyCycleCluster 包络控件
  addAndMakeVisible(dutyCycleClusterEnvelopeCanvas);
  addAndMakeVisible(dutyCycleClusterEnvelopeLabel);
  addAndMakeVisible(dutyCycleClusterEnvelopeYMinSlider);
  addAndMakeVisible(dutyCycleClusterEnvelopeYMinLabel);
  addAndMakeVisible(dutyCycleClusterEnvelopeYMaxSlider);
  addAndMakeVisible(dutyCycleClusterEnvelopeYMaxLabel);
  addAndMakeVisible(dutyCycleClusterDepthSlider);
  addAndMakeVisible(dutyCycleClusterDepthLabel);
  addAndMakeVisible(dutyCycleClusterEnvelopeClearButton);
  addAndMakeVisible(dutyCycleClusterEnvelopeRandomButton);
  addAndMakeVisible(dutyCycleClusterEnvelopeLoadFileButton);
  addAndMakeVisible(dutyCycleClusterEnvControlRow);
  addAndMakeVisible(dutyCycleClusterEnvRangeRow);
  // 将子控件添加到 dutyCycleCluster 容器中
  dutyCycleClusterEnvControlRow.addAndMakeVisible(dutyCycleClusterEnvelopeClearButton);
  dutyCycleClusterEnvControlRow.addAndMakeVisible(dutyCycleClusterEnvelopeRandomButton);
  dutyCycleClusterEnvControlRow.addAndMakeVisible(dutyCycleClusterEnvelopeLoadFileButton);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeYMinSlider);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeYMinLabel);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeYMaxSlider);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeYMaxLabel);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeScaleSlider);
  dutyCycleClusterEnvRangeRow.addAndMakeVisible(dutyCycleClusterEnvelopeScaleLabel);

  // envelope
  addAndMakeVisible(attackSlider);
  addAndMakeVisible(decaySlider);
  addAndMakeVisible(sustainSlider);
  addAndMakeVisible(releaseSlider);

  addAndMakeVisible(attackLabel);
  addAndMakeVisible(decayLabel);
  addAndMakeVisible(sustainLabel);
  addAndMakeVisible(releaseLabel);

  // mask
  addAndMakeVisible(maskComboBoxLabel);
  addAndMakeVisible(maskOptionComboBox);
  addAndMakeVisible(burstMaskLabel);
  addAndMakeVisible(burstMaskTextEditor);
  addAndMakeVisible(euclidStepSlider);
  addAndMakeVisible(euclidStepLabel);
  addAndMakeVisible(euclidHitSlider);
  addAndMakeVisible(euclidHitLabel);
  addAndMakeVisible(stochasticMaskLabel);
  addAndMakeVisible(stochasticMaskTextEditor);
}

/**
 * set ui style
 */
void AudioPluginAudioProcessorEditor::setUIStyle() {
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultFmData{};
  defaultFmData.fill(0.0f);

  // output
  outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
  outputGainSlider.setTextValueSuffix(" (db)");

  outputGainLabel.setText("Output", juce::dontSendNotification);

  // bpm
  bpmSlider.setRange(30.0, 300.0, 1.0); // 合理BPM范围
  bpmSlider.setTextValueSuffix(" BPM");
  bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal); // 或 Rotary
  bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

  // train
  trainLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  trainLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
  trainLenSlider.setTextValueSuffix(" (beat)");

  trainLenLabel.setText("Train Period", juce::dontSendNotification);
  // trainLenLabel.attachToComponent(&trainLenSlider, true);

  trainDutyCycleLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  trainDutyCycleLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
  trainDutyCycleLenSlider.setTextValueSuffix(" (count)");

  trainDutyCycleLenLabel.setText("Train Duty Cycle", juce::dontSendNotification);
  // trainDutyCycleLenLabel.attachToComponent(&trainDutyCycleLenSlider, true);

  trainSilenceLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  trainSilenceLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
  trainSilenceLenSlider.setTextValueSuffix(" (count)");

  trainSilenceLenLabel.setText("Train Silence", juce::dontSendNotification);

  // ========================== Pg Waveform 包络绘制控件样式 ==========================
  pgWaveformEnvelopeLabel.setText("Pg Waveform (Drawn)", juce::dontSendNotification);
  pgWaveformEnvelopeCanvas.setYAxisRange(-1.0f, 1.0f);
  pgWaveformEnvelopeCanvas.addChangeListener(this);
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultPgWaveformData;
  for (int i = 0; i < (int)defaultPgWaveformData.size(); ++i) {
    float x = static_cast<float>(i) / (defaultPgWaveformData.size() - 1);
    defaultPgWaveformData[i] = std::sin(2.0f * juce::MathConstants<float>::pi * x);
  }
  pgWaveformEnvelopeCanvas.setEnvelopeData(defaultPgWaveformData);

  pgWaveformEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  pgWaveformEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  pgWaveformEnvelopeScaleSlider.setRange(0.1, 5.0, 0.01);
  pgWaveformEnvelopeScaleSlider.setValue(1.0);
  pgWaveformEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  // ========================== AM 包络绘制控件样式 ==========================
  ampEnvelopeLabel.setText("AM Envelope", juce::dontSendNotification);

  ampEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeYMinSlider.setRange(0.0, 0.0, 0.0);
  ampEnvelopeYMinSlider.setValue(0.01);
  ampEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  ampEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeYMaxSlider.setRange(0.1, 1.0, 0.1);
  ampEnvelopeYMaxSlider.setValue(1.0);
  ampEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  ampLfoDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  ampLfoDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  ampLfoDepthSlider.setTextValueSuffix("");
  ampLfoDepthSlider.setRange(0.0, 1.0, 0.01);
  ampLfoDepthLabel.setText("AM Depth", juce::dontSendNotification);

  ampEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeScaleSlider.setRange(0.1, 5.0, 0.01);
  ampEnvelopeScaleSlider.setValue(1.0);
  ampEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  ampEnvelopeCanvas.setYAxisRange(0.1f, 1.0f);

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultAmData{};
  defaultAmData.fill(1.0f);
  ampEnvelopeCanvas.setEnvelopeData(defaultAmData);

  // ========================== FM 包络绘制控件样式 ==========================
  fmEnvelopeLabel.setText("FM Envelope", juce::dontSendNotification);

  fmEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeYMinSlider.setRange(1.0f, 1000.0f, 1.0f);
  fmEnvelopeYMinSlider.setValue(1.0f);
  fmEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  fmEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeYMaxSlider.setRange(1000.0, 3700, 1);
  fmEnvelopeYMaxSlider.setValue(3700);
  fmEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  fmEnvelopeDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  fmEnvelopeDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  fmEnvelopeDepthSlider.setTextValueSuffix("");
  fmEnvelopeDepthSlider.setRange(0.0, 1.0, 0.01);
  fmEnvelopeDepthLabel.setText("Depth", juce::dontSendNotification);

  fmEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeScaleSlider.setRange(0.1, 5.0, 0.01);
  fmEnvelopeScaleSlider.setValue(1.0);
  fmEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  fmEnvelopeCanvas.setYAxisRange(-24.0f * 3, 24.0f * 3);

  // FM 包络默认值为 0.0（无调制）
  fmEnvelopeCanvas.setEnvelopeData(defaultFmData);

  // ========================== FM lfo 绘制控件样式 ==========================
  fmLfoLabel.setText("FM Lfo", juce::dontSendNotification);

  fmLfoYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmLfoYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmLfoYMinSlider.setRange(0.1f, 1.0f, 0.1f);
  fmLfoYMinSlider.setValue(1.0f);
  fmLfoYMinLabel.setText("Min", juce::dontSendNotification);

  fmLfoYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmLfoYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmLfoYMaxSlider.setRange(2.0, 10, 0.1);
  fmLfoYMaxSlider.setValue(10);
  fmLfoYMaxLabel.setText("Max", juce::dontSendNotification);

  fmLfoDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  fmLfoDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  fmLfoDepthSlider.setTextValueSuffix("");
  fmLfoDepthSlider.setRange(0.0, 1.0, 0.01);
  fmLfoDepthLabel.setText("Depth", juce::dontSendNotification);

  fmLfoScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmLfoScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmLfoScaleSlider.setRange(0.1, 5.0, 0.01);
  fmLfoScaleSlider.setValue(1.0);
  fmLfoScaleLabel.setText("Scale", juce::dontSendNotification);

  fmLfoCanvas.setYAxisRange(-24.0f * 3, 24.0f * 3);

  // FM 包络默认值为 0.0（无调制）
  defaultFmData.fill(1.0f);
  fmLfoCanvas.setEnvelopeData(defaultFmData);

  // ========================== pulsar duty cycle ratio 包络绘制控件样式 ==========================
  dutyCycleRatioEnvelopeLabel.setText("Duty Cycle Ratio Envelope", juce::dontSendNotification);

  dutyCycleRatioEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleRatioEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleRatioEnvelopeYMinSlider.setRange(0.01, 0.01, 0.01);
  dutyCycleRatioEnvelopeYMinSlider.setValue(0.0);
  dutyCycleRatioEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  dutyCycleRatioEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleRatioEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleRatioEnvelopeYMaxSlider.setRange(0.1, 1.0, 0.01);
  dutyCycleRatioEnvelopeYMaxSlider.setValue(1.0);
  dutyCycleRatioEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  dutyCycleRatioDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  dutyCycleRatioDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  dutyCycleRatioDepthSlider.setTextValueSuffix("");
  dutyCycleRatioDepthSlider.setRange(0.0, 1.0, 0.0);
  dutyCycleRatioDepthLabel.setText("Depth", juce::dontSendNotification);

  dutyCycleRatioEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleRatioEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleRatioEnvelopeScaleSlider.setRange(0.1, 5.0, 0.01);
  dutyCycleRatioEnvelopeScaleSlider.setValue(1.0);
  dutyCycleRatioEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  dutyCycleRatioEnvelopeCanvas.setYAxisRange(0.0f, 1.0f);

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultdutyCycleRatioData{};
  defaultdutyCycleRatioData.fill(0.5f);
  dutyCycleRatioEnvelopeCanvas.setEnvelopeData(defaultdutyCycleRatioData);

  // ========================== pulsar duty cycle Cluster 包络绘制控件样式 ==========================
  dutyCycleClusterEnvelopeLabel.setText("Duty Cycle Cluster Envelope", juce::dontSendNotification);

  dutyCycleClusterEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeYMinSlider.setRange(0.1, 1.0, 0.01);
  dutyCycleClusterEnvelopeYMinSlider.setValue(1.0);
  dutyCycleClusterEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  dutyCycleClusterEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeYMaxSlider.setRange(2.0, 16.0, 0.01);
  dutyCycleClusterEnvelopeYMaxSlider.setValue(16.0);
  dutyCycleClusterEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  dutyCycleClusterDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  dutyCycleClusterDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  dutyCycleClusterDepthSlider.setTextValueSuffix("");
  dutyCycleClusterDepthSlider.setRange(1.0, 16.0, 1);
  dutyCycleClusterDepthLabel.setText("Depth", juce::dontSendNotification);

  dutyCycleClusterEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeScaleSlider.setRange(0.1, 5.0, 0.01);
  dutyCycleClusterEnvelopeScaleSlider.setValue(1.0);
  dutyCycleClusterEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  dutyCycleClusterEnvelopeCanvas.setYAxisRange(1.0f, 16.0f);

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> dutyCycleClusterData{};
  dutyCycleClusterData.fill(1.0f);
  dutyCycleClusterEnvelopeCanvas.setEnvelopeData(dutyCycleClusterData);

  // envelope
  attackSlider.setSliderStyle(juce::Slider::LinearVertical);
  attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  attackSlider.setTextValueSuffix("");

  attackLabel.setText("Attack", juce::dontSendNotification);

  decaySlider.setSliderStyle(juce::Slider::LinearVertical);
  decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  decaySlider.setTextValueSuffix("");

  decayLabel.setText("Decay", juce::dontSendNotification);

  sustainSlider.setSliderStyle(juce::Slider::LinearVertical);
  sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  sustainSlider.setTextValueSuffix("");

  sustainLabel.setText("Sustain", juce::dontSendNotification);

  releaseSlider.setSliderStyle(juce::Slider::LinearVertical);
  releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  releaseSlider.setTextValueSuffix("");

  releaseLabel.setText("Release", juce::dontSendNotification);

  // masking
  maskComboBoxLabel.setText("Mask Mode", juce::dontSendNotification);
  maskOptionComboBox.addItem("Off", static_cast<int>(why::MaskOptionEnum::Off) + 1);
  // item id不能为0，但是parameter获取到的值是从0开始
  maskOptionComboBox.addItem("Burst Mask", static_cast<int>(why::MaskOptionEnum::BurstMask) + 1);
  maskOptionComboBox.addItem("Euclid Mask", static_cast<int>(why::MaskOptionEnum::EuclidMask) + 1);
  maskOptionComboBox.addItem("Stochastic Mask", static_cast<int>(why::MaskOptionEnum::StochasticMask) + 1);
  // maskOptionComboBox.setSelectedId(1);

  burstMaskLabel.setText("Burst Mask", juce::dontSendNotification);

  burstMaskTextEditor.setMultiLine(true);
  burstMaskTextEditor.setScrollbarsShown(true);
  burstMaskTextEditor.setPopupMenuEnabled(false);
  burstMaskTextEditor.setReturnKeyStartsNewLine(false);                                       // 按回车不换行（默认也是 false）
  burstMaskTextEditor.setInputRestrictions(256, "01");                                        // 限制只输入0或1
  burstMaskTextEditor.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(256, // 最长 256 个字符（你可以改）
                                                                                         "01" // 只允许字符 '0' 和 '1'
                                                                                         ),
                                     true); // true 表示替换当前 filter
  burstMaskTextEditor.setTextToShowWhenEmpty("Only 0 or 1 can be input", juce::Colours::grey);

  euclidStepLabel.setText("Euclid Step", juce::dontSendNotification);

  euclidStepSlider.setSliderStyle(juce::Slider::LinearVertical);
  euclidStepSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  euclidStepSlider.setRange(1, 32, 1);
  euclidStepSlider.setNumDecimalPlacesToDisplay(0); // 不显示小数

  euclidHitLabel.setText("Euclid Hit", juce::dontSendNotification);

  euclidHitSlider.setSliderStyle(juce::Slider::LinearVertical);
  euclidHitSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  euclidHitSlider.setRange(1, 32, 1);
  euclidHitSlider.setNumDecimalPlacesToDisplay(0); // 不显示小数

  stochasticMaskLabel.setText("Random Mask", juce::dontSendNotification);

  stochasticMaskTextEditor.setMultiLine(true);
  stochasticMaskTextEditor.setReadOnly(true);
  stochasticMaskTextEditor.setScrollbarsShown(true);
  stochasticMaskTextEditor.setPopupMenuEnabled(false);
  stochasticMaskTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
  stochasticMaskTextEditor.setInputRestrictions(0, "01");    // 限制只输入0或1
  stochasticMaskTextEditor.setTextToShowWhenEmpty("Generated mask will be here", juce::Colours::grey);
}

/**
 * Set the attachment to bind the ui element to the audio parameter, thereby
 * ensuring that the latest value of the parameter is synchronized to the ui,
 * such as automation
 */
void AudioPluginAudioProcessorEditor::connectUIAndAudioParameter() {
  // 绑定UI与Parameter
  outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::outputGain, outputGainSlider);

  bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::bpm, bpmSlider);

  // train
  trainLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainLen, trainLenSlider);
  trainDutyCycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainDutyCycleLen, trainDutyCycleLenSlider);
  trainSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainSilenceLen, trainSilenceLenSlider);

  ampLfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::ampLfoDepth, ampLfoDepthSlider);
  fmEnvelopeDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::formantFreqLfoDepth, fmEnvelopeDepthSlider);
  dutyCycleRatioDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::dutyCycleRatioDepth, dutyCycleRatioDepthSlider);
  dutyCycleClusterDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::dutyCycleClusterDepth, dutyCycleClusterDepthSlider);
  fmLfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::fmLfoDepth, fmLfoDepthSlider);

  attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarAttack, attackSlider);
  decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarDecay, decaySlider);
  sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarSustain, sustainSlider);
  releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarRelease, releaseSlider);

  maskOptionComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, why::ParameterID::maskOption, maskOptionComboBox);
  euclidStepDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidSteps, euclidStepSlider);
  euclidHitDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidHits, euclidHitSlider);
}

/**
 * Settings of all ui element event callback methods
 */
void AudioPluginAudioProcessorEditor::initUITriggerEvent() {
  // AM 包络绘制事件：当包络被绘制时，同步到 synth
  ampEnvelopeCanvas.addChangeListener(this);

  // Y轴范围变化时更新包络画布
  ampEnvelopeYMinSlider.onValueChange = [this] {
    float yMin = ampEnvelopeYMinSlider.getValue();
    float yMax = ampEnvelopeYMaxSlider.getValue();
    ampEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeYRange(yMin, yMax); });
  };

  ampEnvelopeYMaxSlider.onValueChange = [this] {
    float yMin = ampEnvelopeYMinSlider.getValue();
    float yMax = ampEnvelopeYMaxSlider.getValue();
    ampEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeYRange(yMin, yMax); });
  };

  ampEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmpEnvelopeScale(static_cast<float>(ampEnvelopeScaleSlider.getValue())); });
  };

  // 清空包络按钮
  ampEnvelopeClearButton.onClick = [this] { ampEnvelopeCanvas.clearEnvelope(1.0f); };

  // 随机生成包络按钮
  ampEnvelopeRandomButton.onClick = [this] { ampEnvelopeCanvas.randomize(); };

  // FM 包络绘制事件：当包络被绘制时，同步到 synth
  fmEnvelopeCanvas.addChangeListener(this);

  // FM Y轴范围变化时更新包络画布
  fmEnvelopeYMinSlider.onValueChange = [this] {
    float yMin = fmEnvelopeYMinSlider.getValue();
    float yMax = fmEnvelopeYMaxSlider.getValue();
    fmEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeYRange(yMin, yMax); });
  };

  fmEnvelopeYMaxSlider.onValueChange = [this] {
    float yMin = fmEnvelopeYMinSlider.getValue();
    float yMax = fmEnvelopeYMaxSlider.getValue();
    fmEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeYRange(yMin, yMax); });
  };

  fmEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmEnvelopeScale(static_cast<float>(fmEnvelopeScaleSlider.getValue())); });
  };

  // 清空 FM 包络按钮
  fmEnvelopeClearButton.onClick = [this] { fmEnvelopeCanvas.clearEnvelope(0.0f); };
  fmEnvelopeRandomButton.onClick = [this] { fmEnvelopeCanvas.randomize(); };

  // fm lfo
  fmLfoCanvas.addChangeListener(this);

  // FM Y轴范围变化时更新包络画布
  fmLfoYMinSlider.onValueChange = [this] {
    float yMin = fmLfoYMinSlider.getValue();
    float yMax = fmLfoYMaxSlider.getValue();
    fmLfoCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoYRange(yMin, yMax); });
  };

  fmLfoYMaxSlider.onValueChange = [this] {
    float yMin = fmLfoYMinSlider.getValue();
    float yMax = fmLfoYMaxSlider.getValue();
    fmLfoCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoYRange(yMin, yMax); });
  };

  fmLfoScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoScale(static_cast<float>(fmLfoScaleSlider.getValue())); });
  };

  // 清空 FM 按钮
  fmLfoClearButton.onClick = [this] { fmLfoCanvas.clearEnvelope(1.0f); };
  fmLfoRandomButton.onClick = [this] { fmLfoCanvas.randomize(); };

  // =================== duty cycle ratio 包络绘制事件：当包络被绘制时，同步到 synth ===================
  dutyCycleRatioEnvelopeCanvas.addChangeListener(this);

  // Y轴范围变化时更新包络画布
  dutyCycleRatioEnvelopeYMinSlider.onValueChange = [this] {
    float yMin = dutyCycleRatioEnvelopeYMinSlider.getValue();
    float yMax = dutyCycleRatioEnvelopeYMaxSlider.getValue();
    dutyCycleRatioEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeYRange(yMin, yMax); });
  };

  dutyCycleRatioEnvelopeYMaxSlider.onValueChange = [this] {
    float yMin = dutyCycleRatioEnvelopeYMinSlider.getValue();
    float yMax = dutyCycleRatioEnvelopeYMaxSlider.getValue();
    dutyCycleRatioEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeYRange(yMin, yMax); });
  };

  dutyCycleRatioEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback(
        [&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleRatioEnvelopeScale(static_cast<float>(dutyCycleRatioEnvelopeScaleSlider.getValue())); });
  };

  // 清空包络按钮
  dutyCycleRatioEnvelopeClearButton.onClick = [this] { dutyCycleRatioEnvelopeCanvas.clearEnvelope(0.5f); };
  dutyCycleRatioEnvelopeRandomButton.onClick = [this] { dutyCycleRatioEnvelopeCanvas.randomize(); };
  // ===============================================================================================

  // =================== duty cycle cluster 包络绘制事件：当包络被绘制时，同步到 synth ===================
  dutyCycleClusterEnvelopeCanvas.addChangeListener(this);

  // Y轴范围变化时更新包络画布
  dutyCycleClusterEnvelopeYMinSlider.onValueChange = [this] {
    float yMin = dutyCycleClusterEnvelopeYMinSlider.getValue();
    float yMax = dutyCycleClusterEnvelopeYMaxSlider.getValue();
    dutyCycleClusterEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeYRange(yMin, yMax); });
  };

  dutyCycleClusterEnvelopeYMaxSlider.onValueChange = [this] {
    float yMin = dutyCycleClusterEnvelopeYMinSlider.getValue();
    float yMax = dutyCycleClusterEnvelopeYMaxSlider.getValue();
    dutyCycleClusterEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeYRange(yMin, yMax); });
  };

  dutyCycleClusterEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback(
        [&](std::shared_ptr<PulsarSynth> &synth) { synth->setDutyCycleClusterEnvelopeScale(static_cast<float>(dutyCycleClusterEnvelopeScaleSlider.getValue())); });
  };

  // 清空包络按钮
  dutyCycleClusterEnvelopeClearButton.onClick = [this] { dutyCycleClusterEnvelopeCanvas.clearEnvelope(1.0f); };
  dutyCycleClusterEnvelopeRandomButton.onClick = [this] { dutyCycleClusterEnvelopeCanvas.randomize(); };
  // ===============================================================================================

  pgWaveformEnvelopeClearButton.onClick = [this] {
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> sineData;
    for (int i = 0; i < (int)sineData.size(); ++i) {
      float x = static_cast<float>(i) / (sineData.size() - 1);
      sineData[i] = std::sin(2.0f * juce::MathConstants<float>::pi * x); // [-1, 1]
    }
    pgWaveformEnvelopeCanvas.setEnvelopeData(sineData);
  };
  pgWaveformEnvelopeRandomButton.onClick = [this] { pgWaveformEnvelopeCanvas.randomize(); };

  pgWaveformEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback(
        [&](std::shared_ptr<PulsarSynth> &synth) { synth->setPgWaveformEnvelopeScale(static_cast<float>(pgWaveformEnvelopeScaleSlider.getValue())); });
  };

  pgWaveformLoadFileButton.onClick = [this] {
    auto chooser = std::make_shared<juce::FileChooser>("Load audio file as waveform", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser &fc) {
      auto result = fc.getResult();
      if (!result.existsAsFile())
        return;

      juce::AudioFormatManager formatManager;
      formatManager.registerBasicFormats();
      std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(result));
      if (!reader)
        return;

      constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;
      int64 numSrcSamples = reader->lengthInSamples;
      if (numSrcSamples <= 0)
        return;

      int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
      juce::AudioBuffer<float> srcBuffer(1, readSamples);
      reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

      // 线性重采样到 targetSize 点
      std::array<float, targetSize> waveData;
      const float *src = srcBuffer.getReadPointer(0);
      int srcLen = srcBuffer.getNumSamples();
      for (int i = 0; i < targetSize; ++i) {
        float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
        int idx0 = static_cast<int>(srcIdx);
        int idx1 = std::min(idx0 + 1, srcLen - 1);
        float frac = srcIdx - idx0;
        waveData[i] = juce::jlimit(-1.0f, 1.0f, src[idx0] + frac * (src[idx1] - src[idx0]));
      }

      juce::MessageManager::callAsync([this, waveData] { pgWaveformEnvelopeCanvas.setEnvelopeData(waveData); });
    });
  };
  // ===============================================================================================

  // 通用音频文件加载到 envelope 的 lambda
  auto loadAudioToEnvelope = [this](EnvelopeCanvas &canvas) {
    auto chooser = std::make_shared<juce::FileChooser>("Load audio file as envelope", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser, &canvas](const juce::FileChooser &fc) {
      auto result = fc.getResult();
      if (!result.existsAsFile())
        return;

      juce::AudioFormatManager formatManager;
      formatManager.registerBasicFormats();
      std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(result));
      if (!reader)
        return;

      constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;
      int64 numSrcSamples = reader->lengthInSamples;
      if (numSrcSamples <= 0)
        return;

      int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
      juce::AudioBuffer<float> srcBuffer(1, readSamples);
      reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

      std::array<float, targetSize> waveData;
      const float *src = srcBuffer.getReadPointer(0);
      int srcLen = srcBuffer.getNumSamples();
      auto range = canvas.getYAxisRange();
      for (int i = 0; i < targetSize; ++i) {
        float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
        int idx0 = static_cast<int>(srcIdx);
        int idx1 = std::min(idx0 + 1, srcLen - 1);
        float frac = srcIdx - idx0;
        float sample = src[idx0] + frac * (src[idx1] - src[idx0]);
        // 将 [-1, 1] 映射到 canvas 当前 Y 轴范围
        float mapped = range.first + (sample + 1.0f) * 0.5f * (range.second - range.first);
        waveData[i] = juce::jlimit(range.first, range.second, mapped);
      }

      juce::MessageManager::callAsync([&canvas, waveData] { canvas.setEnvelopeData(waveData); });
    });
  };

  ampEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(ampEnvelopeCanvas); };
  fmEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(fmEnvelopeCanvas); };
  dutyCycleRatioEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(dutyCycleRatioEnvelopeCanvas); };
  dutyCycleClusterEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(dutyCycleClusterEnvelopeCanvas); };
  fmLfoLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(fmLfoCanvas); };

  // ===============================================================================================

  // burst mask text editor回车，没有attachment，需要手动更新synth状态
  burstMaskTextEditor.onReturnKey = [&] {
    juce::String currentBurstMaskText = burstMaskTextEditor.getText(); // 获取编辑框中的文本

    // 刷新synth的burst mask标记
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->refreshBurstMask(currentBurstMaskText); });

    // 保存到property，在load
    // preset时，可从parameterChanged监听中获得property值，从而恢复状态
    if (processorRef.apvts.state.getProperty(why::PropertyID::burstMask) != currentBurstMaskText) {
      processorRef.apvts.state.setProperty(why::PropertyID::burstMask, currentBurstMaskText, nullptr);
    }

    // color effect
    juce::Colour originalColour = burstMaskTextEditor.findColour(juce::TextEditor::backgroundColourId);
    // Temporary highlighting
    burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::mediumaquamarine);
    burstMaskTextEditor.repaint();
    // recovery
    juce::Timer::callAfterDelay(300, [&, originalColour]() {
      burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, originalColour);
      burstMaskTextEditor.repaint();
    });
  };

  // euclid联动设置：始终保持hit<=step
  euclidStepSlider.onValueChange = [&] { rebalanceStepHitValueDisplay(); };
  euclidHitSlider.onValueChange = [&] { rebalanceStepHitValueDisplay(); };

  // 强制更新synth依赖的bpm
  bpmSlider.onValueChange = [&] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->forceRefreshBpmAndRebuildTrain(bpmSlider.getValue()); });
  };

  maskOptionComboBox.onChange = [&] {
    // 只在选中stochastic mask时才展示生成的随机mask
    if (maskOptionComboBox.getSelectedItemIndex() == static_cast<int>(why::MaskOptionEnum::StochasticMask)) {
      // stochastic mask默认采用第一个voice的
      std::string maskStrStd = processorRef.getPulsarSynthEngine().getCurrentPulsarSynth()->getStochasticMaskStr();
      juce::String newText = juce::String(maskStrStd);

      if (stochasticMaskTextEditor.getText() != newText) {
        stochasticMaskTextEditor.setText(newText, juce::dontSendNotification);
      }
    }
  };
}

/**
 * euclid linkage setting: When the euclid step changes, the value range of
 * euclid hit is automatically adjusted
 */
void AudioPluginAudioProcessorEditor::rebalanceStepHitValueDisplay() {
  double stepValue = euclidStepSlider.getValue();
  double hitValue = euclidHitSlider.getValue();

  // 更新 sliderB 的最大值
  if (stepValue <= 1) {
    // juce的逻辑：range不可以设置为1，1，必须满足max>min
    // 所以step为1，hit不设为1了，直接禁用
    euclidHitSlider.setValue(1);
    euclidHitSlider.setEnabled(false);
  } else {
    euclidHitSlider.setEnabled(true);
    euclidHitSlider.setRange(euclidHitSlider.getMinimum(), stepValue);
  }

  if (hitValue > stepValue)
    euclidHitSlider.setValue(stepValue,
                             juce::dontSendNotification); // 避免无限触发
}

/**
 * 设置窗口大小
 */
void AudioPluginAudioProcessorEditor::setWindowSize() {
  // Make sure that before the constructor has finished, you've set the editor's
  // size to whatever you need it to be. setSize(1200, 600);
  // 允许窗口被用户调整大小
  setResizable(true, true);
  // 设置最小和最大尺寸
  setResizeLimits(1024, 600, 1920, 1080);
}
