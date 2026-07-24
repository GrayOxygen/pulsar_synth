//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/LfoPage.h"
#include <array>
#include <atomic>

void LfoPage::connectUIAndAudioParameter() {
  fmLfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::fmLfoDepth, fmLfoDepthSlider);
  amLfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::amLfoDepth, amLfoDepthSlider);
}

LfoPage::LfoPage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &state) : processorRef(p), apvts(state) {
  makeVisible();
  setUIStyle();
  initUITriggerEvent();
  connectUIAndAudioParameter();
}

void LfoPage::resized() {
  juce::FlexBox mainFlexBox;
  mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

  // ================== FM Lfo 包络行 - 水平排列 ==================
  juce::FlexBox fmLfoRowFlexBox;
  fmLfoRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  fmLfoRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  fmLfoRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // Depth slider 列
  juce::FlexBox fmLfoDepthFlexBox;
  fmLfoDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  fmLfoDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  fmLfoDepthFlexBox.items.add(juce::FlexItem(fmLfoDepthLabel).withFlex(1).withMaxHeight(20));
  fmLfoDepthFlexBox.items.add(juce::FlexItem(fmLfoDepthSlider).withFlex(2.0));
  fmLfoRowFlexBox.items.add(juce::FlexItem(fmLfoDepthFlexBox).withFlex(0.3f));

  // FM Lfo 包络列
  juce::FlexBox fmLfoEnvelopeFlexBox;
  fmLfoEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  fmLfoEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  fmLfoEnvelopeFlexBox.items.add(juce::FlexItem(fmLfoLabel).withFlex(0.5).withMaxHeight(18));
  fmLfoEnvelopeFlexBox.items.add(juce::FlexItem(fmLfoControlRow).withFlex(0.5).withMaxHeight(24));
  fmLfoEnvelopeFlexBox.items.add(juce::FlexItem(fmLfoRangeRow).withFlex(0.5).withMaxHeight(32));
  fmLfoEnvelopeFlexBox.items.add(juce::FlexItem(fmLfoCanvas).withFlex(2.0).withMinHeight(120));
  fmLfoRowFlexBox.items.add(juce::FlexItem(fmLfoEnvelopeFlexBox).withFlex(1.0f));

  // ================== am Lfo 包络行 - 水平排列 ==================
  juce::FlexBox amLfoRowFlexBox;
  amLfoRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  amLfoRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  amLfoRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  // Depth slider 列
  juce::FlexBox amLfoDepthFlexBox;
  amLfoDepthFlexBox.flexDirection = juce::FlexBox::Direction::column;
  amLfoDepthFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  amLfoDepthFlexBox.items.add(juce::FlexItem(amLfoDepthLabel).withFlex(1).withMaxHeight(20));
  amLfoDepthFlexBox.items.add(juce::FlexItem(amLfoDepthSlider).withFlex(2.0));
  amLfoRowFlexBox.items.add(juce::FlexItem(amLfoDepthFlexBox).withFlex(0.3f));

  // am Lfo 包络列
  juce::FlexBox amLfoEnvelopeFlexBox;
  amLfoEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  amLfoEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  amLfoEnvelopeFlexBox.items.add(juce::FlexItem(amLfoLabel).withFlex(0.5).withMaxHeight(18));
  amLfoEnvelopeFlexBox.items.add(juce::FlexItem(amLfoControlRow).withFlex(0.5).withMaxHeight(24));
  amLfoEnvelopeFlexBox.items.add(juce::FlexItem(amLfoRangeRow).withFlex(0.5).withMaxHeight(32));
  amLfoEnvelopeFlexBox.items.add(juce::FlexItem(amLfoCanvas).withFlex(2.0).withMinHeight(120));
  amLfoRowFlexBox.items.add(juce::FlexItem(amLfoEnvelopeFlexBox).withFlex(1.0f));

  mainFlexBox.items.add(juce::FlexItem(fmLfoRowFlexBox).withFlex(2.0).withMargin({20, 20, 20, 20}));
  mainFlexBox.items.add(juce::FlexItem(amLfoRowFlexBox).withFlex(2.0).withMargin({20, 20, 20, 20}));
  mainFlexBox.performLayout(getLocalBounds());

  // 为 FM lfo 容器内的控件设置布局
  auto fmLfoControlBounds = fmLfoControlRow.getLocalBounds();
  fmLfoClearButton.setBounds(fmLfoControlBounds.removeFromLeft(fmLfoControlBounds.getWidth() * 0.333f));
  fmLfoRandomButton.setBounds(fmLfoControlBounds.removeFromLeft(fmLfoControlBounds.getWidth() * 0.5f));
  fmLfoLoadFileButton.setBounds(fmLfoControlBounds);

  auto fmLfoRangeBounds = fmLfoRangeRow.getLocalBounds();
  int fmLfoLabelWidth = 30;
  int fmLfoSliderWidth = juce::jmax(20, (fmLfoRangeBounds.getWidth() - fmLfoLabelWidth * 3) / 2);
  fmLfoYMinLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  fmLfoYMinSlider.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoSliderWidth));
  fmLfoYMaxLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  fmLfoYMaxSlider.setBounds(fmLfoRangeBounds);
  // fmLfoScaleLabel.setBounds(fmLfoRangeBounds.removeFromLeft(fmLfoLabelWidth));
  // fmLfoScaleSlider.setBounds(fmLfoRangeBounds);

  // am
  auto amLfoControlBounds = amLfoControlRow.getLocalBounds();
  amLfoClearButton.setBounds(amLfoControlBounds.removeFromLeft(amLfoControlBounds.getWidth() * 0.333f));
  amLfoRandomButton.setBounds(amLfoControlBounds.removeFromLeft(amLfoControlBounds.getWidth() * 0.5f));
  amLfoLoadFileButton.setBounds(amLfoControlBounds);

  auto amLfoRangeBounds = amLfoRangeRow.getLocalBounds();
  int amLfoLabelWidth = 30;
  int amLfoSliderWidth = juce::jmax(20, (amLfoRangeBounds.getWidth() - amLfoLabelWidth * 3) / 3);
  amLfoYMinLabel.setBounds(amLfoRangeBounds.removeFromLeft(amLfoLabelWidth));
  amLfoYMinSlider.setBounds(amLfoRangeBounds.removeFromLeft(amLfoSliderWidth));
  amLfoYMaxLabel.setBounds(amLfoRangeBounds.removeFromLeft(amLfoLabelWidth));
  amLfoYMaxSlider.setBounds(amLfoRangeBounds.removeFromLeft(amLfoSliderWidth));
  amLfoScaleLabel.setBounds(amLfoRangeBounds.removeFromLeft(amLfoLabelWidth));
  amLfoScaleSlider.setBounds(amLfoRangeBounds);
}

void LfoPage::makeVisible() {
  // 将子控件添加到 FM lfo 容器中
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

  fmLfoControlRow.addAndMakeVisible(fmLfoClearButton);
  fmLfoControlRow.addAndMakeVisible(fmLfoRandomButton);
  fmLfoControlRow.addAndMakeVisible(fmLfoLoadFileButton);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMinSlider);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMinLabel);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMaxSlider);
  fmLfoRangeRow.addAndMakeVisible(fmLfoYMaxLabel);
  // fmLfoRangeRow.addAndMakeVisible(fmLfoScaleSlider);
  // fmLfoRangeRow.addAndMakeVisible(fmLfoScaleLabel);

  // 将子控件添加到 AM lfo 容器中
  addAndMakeVisible(amLfoCanvas);
  addAndMakeVisible(amLfoLabel);
  addAndMakeVisible(amLfoYMinSlider);
  addAndMakeVisible(amLfoYMinLabel);
  addAndMakeVisible(amLfoYMaxSlider);
  addAndMakeVisible(amLfoYMaxLabel);
  addAndMakeVisible(amLfoDepthSlider);
  addAndMakeVisible(amLfoDepthLabel);
  addAndMakeVisible(amLfoClearButton);
  addAndMakeVisible(amLfoRandomButton);
  addAndMakeVisible(amLfoLoadFileButton);
  addAndMakeVisible(amLfoControlRow);
  addAndMakeVisible(amLfoRangeRow);

  amLfoControlRow.addAndMakeVisible(amLfoClearButton);
  amLfoControlRow.addAndMakeVisible(amLfoRandomButton);
  amLfoControlRow.addAndMakeVisible(amLfoLoadFileButton);
  amLfoRangeRow.addAndMakeVisible(amLfoYMinSlider);
  amLfoRangeRow.addAndMakeVisible(amLfoYMinLabel);
  amLfoRangeRow.addAndMakeVisible(amLfoYMaxSlider);
  amLfoRangeRow.addAndMakeVisible(amLfoYMaxLabel);
  amLfoRangeRow.addAndMakeVisible(amLfoScaleSlider);
  amLfoRangeRow.addAndMakeVisible(amLfoScaleLabel);
}

void LfoPage::setUIStyle() {
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData{};
  // ========================== FM lfo 绘制控件样式 ==========================
  fmLfoLabel.setText("FM Lfo", juce::dontSendNotification);

  fmLfoYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmLfoYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmLfoYMinSlider.setRange(-24.0f, 0.0f, 1.0f);
  fmLfoYMinSlider.setValue(-24.0f);
  fmLfoYMinLabel.setText("Min", juce::dontSendNotification);

  fmLfoYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  fmLfoYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  fmLfoYMaxSlider.setRange(0.0f, 24.0f * 3.0f, 1.0f);
  fmLfoYMaxSlider.setValue(24);
  fmLfoYMaxLabel.setText("Max", juce::dontSendNotification);

  fmLfoDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  fmLfoDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  fmLfoDepthSlider.setTextValueSuffix("");
  fmLfoDepthSlider.setRange(0.01, 1.0, 0.01);
  fmLfoDepthSlider.setValue(1.0f);
  fmLfoDepthLabel.setText("FM LFO Depth", juce::dontSendNotification);

  // fmLfoScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  // fmLfoScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  // fmLfoScaleSlider.setRange(1.0, 6.0, 0.01);
  // fmLfoScaleSlider.setValue(1.0);
  // fmLfoScaleLabel.setText("Scale", juce::dontSendNotification);

  fmLfoCanvas.setYAxisRange(-24.0f * 3.0f, 24.0f * 3.0f);

  defaultData.fill(0.0f);
  fmLfoCanvas.setEnvelopeData(defaultData);

  // ========================== am lfo 绘制控件样式 ==========================
  amLfoLabel.setText("AM Lfo", juce::dontSendNotification);

  amLfoYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  amLfoYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  amLfoYMinSlider.setRange(0.1f, 1.0f, 0.01f);
  amLfoYMinSlider.setValue(0.1f);
  amLfoYMinLabel.setText("Min", juce::dontSendNotification);

  amLfoYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  amLfoYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  amLfoYMaxSlider.setRange(0.0, 1.0, 0.1);
  amLfoYMaxSlider.setValue(1);
  amLfoYMaxLabel.setText("Max", juce::dontSendNotification);

  amLfoDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
  amLfoDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  amLfoDepthSlider.setTextValueSuffix("");
  amLfoDepthSlider.setRange(0.0, 10.0, 0.01);
  amLfoDepthLabel.setText("AM LFO Depth", juce::dontSendNotification);

  amLfoScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  amLfoScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  amLfoScaleSlider.setRange(0.1, 1.0, 0.01);
  amLfoScaleSlider.setValue(1.0);
  amLfoScaleLabel.setText("Scale", juce::dontSendNotification);

  amLfoCanvas.setYAxisRange(0.1f, 1.0f);

  defaultData.fill(1.0f);
  amLfoCanvas.setEnvelopeData(defaultData);
}

void LfoPage::initUITriggerEvent() {
  // fm lfo
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

  // fmLfoScaleSlider.onValueChange = [this] {
  //   processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setFmLfoScale(static_cast<float>(fmLfoScaleSlider.getValue())); });
  // };

  // 清空 FM 按钮
  fmLfoClearButton.onClick = [this] { fmLfoCanvas.clearEnvelope(0.0f); };
  fmLfoRandomButton.onClick = [this] { fmLfoCanvas.randomize(); };

  // am
  // am Y轴范围变化时更新包络画布
  amLfoYMinSlider.onValueChange = [this] {
    float yMin = amLfoYMinSlider.getValue();
    float yMax = amLfoYMaxSlider.getValue();
    amLfoCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmLfoYRange(yMin, yMax); });
  };

  amLfoYMaxSlider.onValueChange = [this] {
    float yMin = amLfoYMinSlider.getValue();
    float yMax = amLfoYMaxSlider.getValue();
    amLfoCanvas.setYAxisRange(yMin, yMax);
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmLfoYRange(yMin, yMax); });
  };

  amLfoScaleSlider.onValueChange = [this] {
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setAmLfoScale(static_cast<float>(amLfoScaleSlider.getValue())); });
  };

  // 清空 am 按钮
  amLfoClearButton.onClick = [this] { amLfoCanvas.clearEnvelope(1.0f); };
  amLfoRandomButton.onClick = [this] { amLfoCanvas.randomize(); };

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

  fmLfoLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(fmLfoCanvas); };
  amLfoLoadFileButton.onClick = [this, loadAudioToEnvelope] { loadAudioToEnvelope(amLfoCanvas); };
}