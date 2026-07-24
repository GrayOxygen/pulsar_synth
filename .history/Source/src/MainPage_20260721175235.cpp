#include "../include/MainPage.h"
#include <array>
#include <atomic>

void MainPage::connectUIAndAudioParameter() {
  // 绑定UI与Parameter
  outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::outputGain, outputGainSlider);
  bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::bpm, bpmSlider);

  // train
  trainLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainLen, trainLenSlider);
  trainDutyCycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainDutyCycleLen, trainDutyCycleLenSlider);
  trainSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainSilenceLen, trainSilenceLenSlider);

  attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarAttack, attackSlider);
  decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarDecay, decaySlider);
  sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarSustain, sustainSlider);
  releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarRelease, releaseSlider);

  maskOptionComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, why::ParameterID::maskOption, maskOptionComboBox);
  euclidStepDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidSteps, euclidStepSlider);
  euclidHitDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidHits, euclidHitSlider);
}

void MainPage::makeVisible() {
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

MainPage::MainPage(AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &state) : processorRef(p), apvts(state) {
  makeVisible();
  setUIStyle();
  initUITriggerEvent();
  connectUIAndAudioParameter();
}

void MainPage::topFlexBox(juce::FlexBox &flexBoxTop, std::shared_ptr<juce::FlexBox> trainLenFlexBox, std::shared_ptr<juce::FlexBox> trainDutyCycleFlexBox,
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

void MainPage::midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox, std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                          std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox, std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox,
                          std::shared_ptr<juce::FlexBox> attackFlexBox, std::shared_ptr<juce::FlexBox> decayFlexBox, std::shared_ptr<juce::FlexBox> sustainFlexBox,
                          std::shared_ptr<juce::FlexBox> releaseFlexBox) {
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

void MainPage::resized() {
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
  mainFlexBox.items.add(juce::FlexItem(pgWaveformRowFlexBox).withFlex(1.5).withMargin({10, 10, 20, 20}));
  mainFlexBox.performLayout(getLocalBounds());

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
}

void MainPage::setUIStyle() {
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

  // ========================== Pg Waveform 包络绘制控件样式 ==========================
  pgWaveformEnvelopeLabel.setText("Pg Waveform (Drawn)", juce::dontSendNotification);
  pgWaveformEnvelopeCanvas.setYAxisRange(-1.0f, 1.0f);
  // pgWaveformEnvelopeCanvas.addChangeListener(this);
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
}

void MainPage::initUITriggerEvent() {
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
    auto chooser = std::make_shared<juce::FileChooser>("Load audio files as wavetable (multi-select)", juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                       "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                         [this, chooser](const juce::FileChooser &fc) {
                           auto results = fc.getResults();
                           if (results.isEmpty())
                             return;

                           juce::AudioFormatManager formatManager;
                           formatManager.registerBasicFormats();

                           // wavetable scanning：每个文件均分为FRAMES_PER_FILE段，每段重采样为一帧(2048点)，
                           // 按文件顺序拼接成帧序列；scanning在文件内morph，并平滑过渡到下一个文件
                           constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;
                           std::vector<std::array<float, targetSize>> frames;

                           for (const auto &file : results) {
                             if (static_cast<int>(frames.size()) >= CommonVoiceSate::WAVETABLE_FRAMES)
                               break;
                             if (!file.existsAsFile())
                               continue;

                             std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
                             if (!reader)
                               continue;

                             int64 numSrcSamples = reader->lengthInSamples;
                             if (numSrcSamples <= 0)
                               continue;

                             int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
                             juce::AudioBuffer<float> srcBuffer(1, readSamples);
                             reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

                             const float *src = srcBuffer.getReadPointer(0);
                             int srcLen = srcBuffer.getNumSamples();
                             int numFileFrames = CommonVoiceSate::FRAMES_PER_FILE;
                             if (srcLen / numFileFrames < 2)
                               numFileFrames = 1; // 文件太短，退化为单帧
                             numFileFrames = std::min(numFileFrames, CommonVoiceSate::WAVETABLE_FRAMES - static_cast<int>(frames.size()));

                             for (int f = 0; f < numFileFrames; ++f) {
                               std::array<float, targetSize> frame;
                               int frameStart = static_cast<int>(static_cast<int64>(srcLen) * f / numFileFrames);
                               int frameEnd = static_cast<int>(static_cast<int64>(srcLen) * (f + 1) / numFileFrames); // exclusive
                               int frameLen = std::max(2, frameEnd - frameStart);
                               for (int i = 0; i < targetSize; ++i) {
                                 float srcIdx = frameStart + static_cast<float>(i) * (frameLen - 1) / (targetSize - 1);
                                 int idx0 = std::min(static_cast<int>(srcIdx), srcLen - 1);
                                 int idx1 = std::min(idx0 + 1, srcLen - 1);
                                 float frac = srcIdx - idx0;
                                 frame[i] = src[idx0] + frac * (src[idx1] - src[idx0]);
                               }
                               frames.push_back(frame);
                             }
                           }

                           if (frames.empty())
                             return;

                           // per-frame RMS归一化：每帧独立归到相同目标RMS，scanning/morph途中响度恒定，
                           // 不同文件/不同段落的响度差异不再带入波表
                           constexpr float targetRms = 0.5f; // 满幅sine的RMS≈0.707，留0.5留出峰值余量
                           for (auto &frame : frames) {
                             double sumSq = 0.0;
                             for (float s : frame)
                               sumSq += static_cast<double>(s) * s;
                             float rms = std::sqrt(static_cast<float>(sumSq / targetSize));
                             if (rms > 1.0e-6f) {
                               float g = targetRms / rms;
                               for (float &s : frame)
                                 s *= g;
                             }
                           }

                           // 全局峰值clamp：若某帧峰因波峰因子高而超出[-1,1]，所有帧统一缩放，
                           // 保持帧间RMS一致性的同时不削顶
                           float peak = 0.0f;
                           for (const auto &frame : frames)
                             for (float s : frame)
                               peak = std::max(peak, std::abs(s));
                           if (peak > 1.0f) {
                             float norm = 1.0f / peak;
                             for (auto &frame : frames)
                               for (float &s : frame)
                                 s *= norm;
                           }

                           juce::MessageManager::callAsync([this, frames] {
                             processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setPgWavetableData(frames); });
                             // canvas只展示第一帧；notify=false，避免canvas listener把wavetable覆盖回单帧
                             pgWaveformEnvelopeCanvas.setEnvelopeData(frames[0], false);
                           });
                         });
  };
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

void MainPage::rebalanceStepHitValueDisplay() {
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
    euclidHitSlider.setValue(stepValue, juce::dontSendNotification); // 避免无限触发
}
