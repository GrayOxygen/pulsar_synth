#include "../include/TrainEnvelopePage.h"
#include <array>
#include <atomic>
TrainEnvelopePage::TrainEnvelopePage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &state) : processorRef(p), apvts(state) {
  makeVisible();
  setUIStyle();
  initUITriggerEvent();
  connectUIAndAudioParameter();
}

void TrainEnvelopePage::connectUIAndAudioParameter() {
  amEnvelopeDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::amEnvelopeDepth, amEnvelopeDepthSlider);
  fmEnvelopeDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::fmEnvelopeDepth, fmEnvelopeDepthSlider);
  panDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::panDepth, panDepthSlider);
  dutyCycleClusterDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::dutyCycleClusterDepth, dutyCycleClusterDepthSlider);
}

void TrainEnvelopePage::makeVisible() {
  // AM 包络控件
  addAndMakeVisible(ampEnvelopeCanvas);
  addAndMakeVisible(ampEnvelopeLabel);
  addAndMakeVisible(ampEnvelopeYMinSlider);
  addAndMakeVisible(ampEnvelopeYMinLabel);
  addAndMakeVisible(ampEnvelopeYMaxSlider);
  addAndMakeVisible(ampEnvelopeYMaxLabel);
  addAndMakeVisible(amEnvelopeDepthSlider);
  addAndMakeVisible(amEnvelopeDepthLabel);
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

  // pan 包络控件
  addAndMakeVisible(panEnvelopeCanvas);
  addAndMakeVisible(panEnvelopeLabel);
  addAndMakeVisible(panEnvelopeYMinSlider);
  addAndMakeVisible(panEnvelopeYMinLabel);
  addAndMakeVisible(panEnvelopeYMaxSlider);
  addAndMakeVisible(panEnvelopeYMaxLabel);
  addAndMakeVisible(panDepthSlider);
  addAndMakeVisible(panDepthLabel);
  addAndMakeVisible(panEnvelopeClearButton);
  addAndMakeVisible(panEnvelopeRandomButton);
  addAndMakeVisible(panEnvelopeLoadFileButton);
  addAndMakeVisible(panEnvControlRow);
  addAndMakeVisible(panEnvRangeRow);
  // 将子控件添加到 pan 容器中
  panEnvControlRow.addAndMakeVisible(panEnvelopeClearButton);
  panEnvControlRow.addAndMakeVisible(panEnvelopeRandomButton);
  panEnvControlRow.addAndMakeVisible(panEnvelopeLoadFileButton);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeYMinSlider);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeYMinLabel);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeYMaxSlider);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeYMaxLabel);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeScaleSlider);
  panEnvRangeRow.addAndMakeVisible(panEnvelopeScaleLabel);

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
  // dutyCycleClusterEnvRangeRow.addAndMakeVisible(envelopeDilationSlider);
  // dutyCycleClusterEnvRangeRow.addAndMakeVisible(envelopeDilationLabel);
}

void TrainEnvelopePage::resized() {
  auto area = getLocalBounds();
  juce::FlexBox mainFlexBox;
  mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

  // ================== AM/FM 包络行 - 在最底部单独一行，水平均分 ==================
  juce::FlexBox amFmRowFlexBox;
  amFmRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  amFmRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  amFmRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // AM 包络占据一半
  juce::FlexBox ampEnvelopeFlexBox;
  ampEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  ampEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  juce::FlexBox amEnvelopeDepthFlexBox;
  amEnvelopeDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  amEnvelopeDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  amEnvelopeDepthFlexBox.items.add(juce::FlexItem(amEnvelopeDepthLabel).withFlex(1).withMaxHeight(20));
  amEnvelopeDepthFlexBox.items.add(juce::FlexItem(amEnvelopeDepthSlider).withFlex(2.0));
  amFmRowFlexBox.items.add(juce::FlexItem(amEnvelopeDepthFlexBox).withFlex(0.3f));

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

  // ================== pan/pulsar cluster 包络行 - 在最底部单独一行，水平均分 ==================
  juce::FlexBox panClusterFlexBox;
  panClusterFlexBox.flexDirection = juce::FlexBox::Direction::row;
  panClusterFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  panClusterFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // cluster box
  juce::FlexBox dutyCycleClusterDepthFlexBox;
  dutyCycleClusterDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleClusterDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleClusterDepthFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthLabel).withFlex(1).withMaxHeight(20));
  dutyCycleClusterDepthFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthSlider).withFlex(2.0));
  panClusterFlexBox.items.add(juce::FlexItem(dutyCycleClusterDepthFlexBox).withFlex(0.3f));

  juce::FlexBox dutyCycleClusterEnvelopeFlexBox;
  dutyCycleClusterEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  dutyCycleClusterEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvControlRow).withFlex(0.5).withMaxHeight(24));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvRangeRow).withFlex(0.5).withMaxHeight(32));
  dutyCycleClusterEnvelopeFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  panClusterFlexBox.items.add(juce::FlexItem(dutyCycleClusterEnvelopeFlexBox).withFlex(1.0f));

  // pan 包络占据一半
  juce::FlexBox panDepthEnvelopeFlexBox;
  panDepthEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  panDepthEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  panDepthEnvelopeFlexBox.items.add(juce::FlexItem(panDepthLabel).withFlex(1).withMaxHeight(20));
  panDepthEnvelopeFlexBox.items.add(juce::FlexItem(panDepthSlider).withFlex(2.0));
  panClusterFlexBox.items.add(juce::FlexItem(panDepthEnvelopeFlexBox).withFlex(0.3f));

  juce::FlexBox panEnvelopeFlexBox;
  panEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  panEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  panEnvelopeFlexBox.items.add(juce::FlexItem(panEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  panEnvelopeFlexBox.items.add(juce::FlexItem(panEnvControlRow).withFlex(0.5).withMaxHeight(24));
  panEnvelopeFlexBox.items.add(juce::FlexItem(panEnvRangeRow).withFlex(0.5).withMaxHeight(32));
  panEnvelopeFlexBox.items.add(juce::FlexItem(panEnvelopeCanvas).withFlex(2.0).withMinHeight(120));
  panClusterFlexBox.items.add(juce::FlexItem(panEnvelopeFlexBox).withFlex(1.0f));

  // Overall combination, withMargin: up, right, down, left
  mainFlexBox.items.add(juce::FlexItem(amFmRowFlexBox).withFlex(2.0).withMargin({10, 10, 20, 20}));
  mainFlexBox.items.add(juce::FlexItem(panClusterFlexBox).withFlex(2.0).withMargin({10, 10, 20, 20}));
  mainFlexBox.performLayout(area);

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
  ampEnvelopeScaleLabel.setBounds(rangeBounds.removeFromLeft(labelWidth * 1.5f));
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
  fmEnvelopeScaleLabel.setBounds(fmRangeBounds.removeFromLeft(fmLabelWidth * 1.5f));
  fmEnvelopeScaleSlider.setBounds(fmRangeBounds);

  // 为 duty cycle cluster 包络容器内的控件设置布局
  auto dutyCycleClusterControlBounds = dutyCycleClusterEnvControlRow.getLocalBounds();
  dutyCycleClusterEnvelopeClearButton.setBounds(dutyCycleClusterControlBounds.removeFromLeft(dutyCycleClusterControlBounds.getWidth() * 0.333f));
  dutyCycleClusterEnvelopeRandomButton.setBounds(dutyCycleClusterControlBounds.removeFromLeft(dutyCycleClusterControlBounds.getWidth() * 0.5f));
  dutyCycleClusterEnvelopeLoadFileButton.setBounds(dutyCycleClusterControlBounds);

  auto dutyCycleClusterRangeBounds = dutyCycleClusterEnvRangeRow.getLocalBounds();
  int clusterLabelWidth = 30;
  int clusterSliderWidth = juce::jmax(20, (dutyCycleClusterRangeBounds.getWidth() - clusterLabelWidth * 4) / 3);
  dutyCycleClusterEnvelopeYMinLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth * 0.95f));
  dutyCycleClusterEnvelopeYMinSlider.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterSliderWidth * 0.95f));
  dutyCycleClusterEnvelopeYMaxLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth * 0.95f));
  dutyCycleClusterEnvelopeYMaxSlider.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterSliderWidth * 0.95f));
  dutyCycleClusterEnvelopeScaleLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth * 1.5f));
  dutyCycleClusterEnvelopeScaleSlider.setBounds(dutyCycleClusterRangeBounds);
  // envelopeDilationLabel.setBounds(dutyCycleClusterRangeBounds.removeFromLeft(clusterLabelWidth));
  // envelopeDilationSlider.setBounds(dutyCycleClusterRangeBounds);

  // 为 pan 包络容器内的控件设置布局
  auto panControlBounds = panEnvControlRow.getLocalBounds();
  panEnvelopeClearButton.setBounds(panControlBounds.removeFromLeft(panControlBounds.getWidth() * 0.333f));
  panEnvelopeRandomButton.setBounds(panControlBounds.removeFromLeft(panControlBounds.getWidth() * 0.5f));
  panEnvelopeLoadFileButton.setBounds(panControlBounds);

  auto panRangeBounds = panEnvRangeRow.getLocalBounds();
  int panLabelWidth = 30;
  int panSliderWidth = juce::jmax(20, (panRangeBounds.getWidth() - panLabelWidth * 3) / 3);
  panEnvelopeYMinLabel.setBounds(panRangeBounds.removeFromLeft(panLabelWidth));
  panEnvelopeYMinSlider.setBounds(panRangeBounds.removeFromLeft(panSliderWidth));
  panEnvelopeYMaxLabel.setBounds(panRangeBounds.removeFromLeft(panLabelWidth));
  panEnvelopeYMaxSlider.setBounds(panRangeBounds.removeFromLeft(panSliderWidth));
  panEnvelopeScaleLabel.setBounds(panRangeBounds.removeFromLeft(panLabelWidth * 1.5f));
  panEnvelopeScaleSlider.setBounds(panRangeBounds);
}

void TrainEnvelopePage::setUIStyle() {
  // ========================== AM 包络绘制控件样式 ==========================
  ampEnvelopeLabel.setText("AM Envelope", juce::dontSendNotification);

  ampEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeYMinSlider.setRange(0.1, 0.1, 0.1);
  ampEnvelopeYMinSlider.setValue(0.1);
  ampEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  ampEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeYMaxSlider.setRange(1.0, 1.0, 1);
  ampEnvelopeYMaxSlider.setValue(1.0);
  ampEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  amEnvelopeDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  amEnvelopeDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  amEnvelopeDepthSlider.setTextValueSuffix("");
  amEnvelopeDepthSlider.setRange(0.0, 1.0, 0.01);
  amEnvelopeDepthLabel.setText("Depth", juce::dontSendNotification);

  ampEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ampEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  ampEnvelopeScaleSlider.setRange(1.0f, 2.0, 0.01);
  ampEnvelopeScaleSlider.setValue(1.0);
  ampEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  ampEnvelopeCanvas.setYAxisRange(0.1f, 1.0f);

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultAmData{};
  defaultAmData.fill(1.0f);
  ampEnvelopeCanvas.setEnvelopeData(defaultAmData);

  // ========================== FM 包络绘制控件样式 ==========================
  fmEnvelopeLabel.setText("Formant Envelope (scan rate in one period)", juce::dontSendNotification);

  fmEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeYMinSlider.setRange(0.1, 0.1, 0.1);
  fmEnvelopeYMinSlider.setValue(0.1);
  fmEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  fmEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeYMaxSlider.setRange(1.0f, 1.0f, 1.0f);
  fmEnvelopeYMaxSlider.setValue(1.0f);
  fmEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  fmEnvelopeDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  fmEnvelopeDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  fmEnvelopeDepthSlider.setTextValueSuffix("");
  fmEnvelopeDepthSlider.setRange(0.01f, 1.0f, 0.1f);
  fmEnvelopeDepthSlider.setValue(1.0f);
  fmEnvelopeDepthLabel.setText("Depth", juce::dontSendNotification);

  fmEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmEnvelopeScaleSlider.setRange(1, 1, 1);
  fmEnvelopeScaleSlider.setValue(1);
  fmEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  fmEnvelopeCanvas.setYAxisRange(0.1f, 1.0f);

  // FM 包络默认值为 0.0（无调制）
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultFmData{};
  defaultFmData.fill(1.0f);
  fmEnvelopeCanvas.setEnvelopeData(defaultFmData);

  // ========================== pan 包络绘制控件样式 (0=左, 0.5=居中, 1=右) ==========================
  panEnvelopeLabel.setText("Pan Envelope", juce::dontSendNotification);

  panEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  panEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  panEnvelopeYMinSlider.setRange(0.0, 0.5, 0.01);
  panEnvelopeYMinSlider.setValue(0.0);
  panEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  panEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  panEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  panEnvelopeYMaxSlider.setRange(0.5, 1.0, 0.01);
  panEnvelopeYMaxSlider.setValue(1.0);
  panEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  panDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  panDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  panDepthSlider.setTextValueSuffix("");
  panDepthSlider.setRange(0.0, 1.0, 0.01);
  panDepthLabel.setText("Depth", juce::dontSendNotification);

  panEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  panEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  panEnvelopeScaleSlider.setRange(0.1, 1.0, 0.01);
  panEnvelopeScaleSlider.setValue(1.0);
  panEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  panEnvelopeCanvas.setYAxisRange(0.0f, 1.0f);

  // 默认0.5=居中
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultPanData{};
  defaultPanData.fill(0.5f);
  panEnvelopeCanvas.setEnvelopeData(defaultPanData);

  // ========================== pulsar duty cycle Cluster 包络绘制控件样式 ==========================
  dutyCycleClusterEnvelopeLabel.setText("Duty Cycle Cluster Envelope (number of pulsar periods)", juce::dontSendNotification);

  dutyCycleClusterEnvelopeYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeYMinSlider.setRange(1, 1, 1);
  dutyCycleClusterEnvelopeYMinSlider.setValue(1);
  dutyCycleClusterEnvelopeYMinLabel.setText("Min", juce::dontSendNotification);

  dutyCycleClusterEnvelopeYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeYMaxSlider.setRange(8, 8, 8);
  dutyCycleClusterEnvelopeYMaxSlider.setValue(8);
  dutyCycleClusterEnvelopeYMaxLabel.setText("Max", juce::dontSendNotification);

  dutyCycleClusterDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  dutyCycleClusterDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  dutyCycleClusterDepthSlider.setTextValueSuffix("");
  dutyCycleClusterDepthSlider.setRange(0.1f, 8.0f, 0.01f);
  dutyCycleClusterDepthSlider.setValue(1.0f);
  dutyCycleClusterDepthLabel.setText("Depth", juce::dontSendNotification);

  dutyCycleClusterEnvelopeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  dutyCycleClusterEnvelopeScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  dutyCycleClusterEnvelopeScaleSlider.setRange(1, 1, 1);
  dutyCycleClusterEnvelopeScaleSlider.setValue(1.0f);
  dutyCycleClusterEnvelopeScaleLabel.setText("Scale", juce::dontSendNotification);

  // envelope dilation：<1包络掐断波形(打击感/宽带)，=1经典pulsar，>1一个包络装更多周期(overlap/稳态)
  // envelopeDilationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  // envelopeDilationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  // envelopeDilationSlider.setRange(0.05, 4.0, 0.01);
  // envelopeDilationSlider.setValue(1.0);
  // envelopeDilationLabel.setText("Dilat", juce::dontSendNotification);

  dutyCycleClusterEnvelopeCanvas.setYAxisRange(1.0f, 8.0f);

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> dutyCycleClusterData{};
  dutyCycleClusterData.fill(1.0f);
  dutyCycleClusterEnvelopeCanvas.setEnvelopeData(dutyCycleClusterData);
}

void TrainEnvelopePage::initUITriggerEvent() {
  // AM 包络绘制事件：当包络被绘制时，同步到 synth ;
  //   ampEnvelopeCanvas.addChangeListener(this);

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
  // fmEnvelopeCanvas.addChangeListener(this);

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
  fmEnvelopeClearButton.onClick = [this] { fmEnvelopeCanvas.clearEnvelope(1.0f); };
  fmEnvelopeRandomButton.onClick = [this] { fmEnvelopeCanvas.randomize(); };

  // =================== pan 包络绘制事件：当包络被绘制时，同步到 synth ===================
  //   panEnvelopeCanvas.addChangeListener(this);

  // Y轴范围变化时更新包络画布
  panEnvelopeYMinSlider.onValueChange = [this] {
    float yMin = panEnvelopeYMinSlider.getValue();
    float yMax = panEnvelopeYMaxSlider.getValue();
    panEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setPanEnvelopeYRange(yMin, yMax); });
  };

  panEnvelopeYMaxSlider.onValueChange = [this] {
    float yMin = panEnvelopeYMinSlider.getValue();
    float yMax = panEnvelopeYMaxSlider.getValue();
    panEnvelopeCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setPanEnvelopeYRange(yMin, yMax); });
  };

  panEnvelopeScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setPanEnvelopeScale(static_cast<float>(panEnvelopeScaleSlider.getValue())); });
  };

  // 清空包络按钮
  panEnvelopeClearButton.onClick = [this] { panEnvelopeCanvas.clearEnvelope(0.5f); };
  panEnvelopeRandomButton.onClick = [this] { panEnvelopeCanvas.randomize(); };
  // ===============================================================================================

  // =================== duty cycle cluster 包络绘制事件：当包络被绘制时，同步到 synth ===================
  //   dutyCycleClusterEnvelopeCanvas.addChangeListener(this);

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

  // envelopeDilationSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setEnvelopeDilation(static_cast<float>(envelopeDilationSlider.getValue())); });
  // };

  // 清空包络按钮
  dutyCycleClusterEnvelopeClearButton.onClick = [this] { dutyCycleClusterEnvelopeCanvas.clearEnvelope(1.0f); };
  dutyCycleClusterEnvelopeRandomButton.onClick = [this] { dutyCycleClusterEnvelopeCanvas.randomize(); };
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
  panEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(panEnvelopeCanvas); };
  dutyCycleClusterEnvelopeLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(dutyCycleClusterEnvelopeCanvas); };

  // ===============================================================================================
}