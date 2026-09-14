#include "../include/MainPage.h"
#include <array>
#include <atomic>
#include <memory>

void MainPage::connectUIAndAudioParameter() {
  // 绑定UI与Parameter
  outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::outputGain, outputGainSlider);
  // bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::bpm, bpmSlider);

  // train
  trainLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainLen, trainLenSlider);
  trainDutyCycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainDutyCycleLen, trainDutyCycleLenSlider);
  trainSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::trainSilenceLen, trainSilenceLenSlider);

  attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarAttack, grainAdsrAttackSlider);
  decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarDecay, grainAdsrDecaySlider);
  sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarSustain, grainAdsrSustainSlider);
  releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::pulsarRelease, grainAdsrReleaseSlider);

  // pulsar extension
  harmonicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::harmonics, harmonicsSlider);
  unisonDetuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::unisonDetune, unisonDetuneSlider);
  unisonWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::unisonWidth, unisonWidthSlider);

  maskOptionComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, why::ParameterID::maskOption, maskOptionComboBox);
  euclidStepDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidSteps, euclidStepSlider);
  euclidHitDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, why::ParameterID::euclidHits, euclidHitSlider);
}

void MainPage::makeVisible() {
  // output gain
  addAndMakeVisible(outputGainLabel);
  addAndMakeVisible(outputGainSlider);

  // bpm 包络控件
  addAndMakeVisible(bpmEnvelopeCanvas);
  addAndMakeVisible(bpmEnvelopeLabel);
  addAndMakeVisible(bpmEnvControlRow);
  bpmEnvControlRow.addAndMakeVisible(bpmEnvelopeClearButton);
  bpmEnvControlRow.addAndMakeVisible(bpmEnvelopeRandomButton);
  bpmEnvControlRow.addAndMakeVisible(bpmLoadFileButton);
  bpmEnvControlRow.addAndMakeVisible(bpmYMinLabel);
  bpmEnvControlRow.addAndMakeVisible(bpmYMinSlider);
  bpmEnvControlRow.addAndMakeVisible(bpmYMaxLabel);
  bpmEnvControlRow.addAndMakeVisible(bpmYMaxSlider);

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
  // addAndMakeVisible(attackSlider);
  // addAndMakeVisible(decaySlider);
  // addAndMakeVisible(sustainSlider);
  // addAndMakeVisible(releaseSlider);

  // addAndMakeVisible(attackLabel);
  // addAndMakeVisible(decayLabel);
  // addAndMakeVisible(sustainLabel);
  // addAndMakeVisible(releaseLabel);

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
  addAndMakeVisible(grainAdsrLabel);
  addAndMakeVisible(grainAdsrAttackSlider);
  addAndMakeVisible(grainAdsrDecaySlider);
  addAndMakeVisible(grainAdsrSustainSlider);
  addAndMakeVisible(grainAdsrReleaseSlider);

  // pulsar extension
  addAndMakeVisible(extendLabel);
  addAndMakeVisible(harmonicsSlider);
  addAndMakeVisible(unisonDetuneSlider);
  addAndMakeVisible(unisonWidthSlider);
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
  // bpmFlexBox->items.add(juce::FlexItem().withFlex(0.1));
  // bpmFlexBox->items.add(juce::FlexItem(bpmSlider).withFlex(1));
  flexBoxTop.items.add(juce::FlexItem(*bpmFlexBox).withFlex(1.0f));
}

void MainPage::midFlexBox(juce::FlexBox &midFlexBox, std::shared_ptr<juce::FlexBox> triggerFlexBox, std::shared_ptr<juce::FlexBox> maskOptionFlexBox, std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                          std::shared_ptr<juce::FlexBox> euclidStepFlexBox, std::shared_ptr<juce::FlexBox> euclidHitFlexBox, std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox,
                          std::shared_ptr<juce::FlexBox> adsrFlexBox, std::shared_ptr<juce::FlexBox> extendFlexBox) {
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
  midFlexBox.items.add(juce::FlexItem(*burstMaskFlexBox).withFlex(1.0f));

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
  midFlexBox.items.add(juce::FlexItem(*euclidHitFlexBox).withFlex(0.7f));

  stochasticMaskFlexBox->flexDirection = juce::FlexBox::Direction::row;
  stochasticMaskFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
  stochasticMaskFlexBox->items.add(juce::FlexItem(stochasticMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
  stochasticMaskFlexBox->items.add(juce::FlexItem(stochasticMaskTextEditor).withFlex(1.0).withMaxWidth(250).withMaxHeight(150));
  // stochasticMaskFlexBox->items.add(juce::FlexItem(grainAdsrLabel).withFlex(0.5).withMaxWidth(50).withMaxHeight(20));
  // stochasticMaskFlexBox->items.add(juce::FlexItem(grainAdsrAttackSlider).withFlex(1.0).withMaxWidth(60).withMaxHeight(80));
  // stochasticMaskFlexBox->items.add(juce::FlexItem(grainAdsrDecaySlider).withFlex(1.0).withMaxWidth(60).withMaxHeight(80));
  // stochasticMaskFlexBox->items.add(juce::FlexItem(grainAdsrSustainSlider).withFlex(1.0).withMaxWidth(60).withMaxHeight(80));
  // stochasticMaskFlexBox->items.add(juce::FlexItem(grainAdsrReleaseSlider).withFlex(1.0).withMaxWidth(60).withMaxHeight(80));
  midFlexBox.items.add(juce::FlexItem(*stochasticMaskFlexBox).withFlex(0.7f));

  // per-grain ADSR窗: random mask后同一行的4个旋钮
  adsrFlexBox->flexDirection = juce::FlexBox::Direction::row;
  adsrFlexBox->items.add(juce::FlexItem(grainAdsrLabel).withFlex(1).withMaxWidth(80).withMaxHeight(20));
  adsrFlexBox->items.add(juce::FlexItem(grainAdsrAttackSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  adsrFlexBox->items.add(juce::FlexItem(grainAdsrDecaySlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  adsrFlexBox->items.add(juce::FlexItem(grainAdsrSustainSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  adsrFlexBox->items.add(juce::FlexItem(grainAdsrReleaseSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));

  midFlexBox.items.add(juce::FlexItem(*adsrFlexBox).withFlex(1.0f));

  // pulsar extension: ADSR后同一行的3个旋钮(谐波层/unison失谐/unison宽度)
  extendFlexBox->flexDirection = juce::FlexBox::Direction::row;
  extendFlexBox->items.add(juce::FlexItem(extendLabel).withFlex(1).withMaxWidth(60).withMaxHeight(20));
  extendFlexBox->items.add(juce::FlexItem(harmonicsSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  extendFlexBox->items.add(juce::FlexItem(unisonDetuneSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  extendFlexBox->items.add(juce::FlexItem(unisonWidthSlider).withFlex(1.0).withMaxWidth(100).withMaxHeight(80));
  midFlexBox.items.add(juce::FlexItem(*extendFlexBox).withFlex(0.9f));

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
  std::shared_ptr<juce::FlexBox> adsrFlexBox = std::make_shared<juce::FlexBox>();
  std::shared_ptr<juce::FlexBox> extendFlexBox = std::make_shared<juce::FlexBox>();
  this->midFlexBox(midFlexBox, row10, row11, row12, row13, row14, row15, adsrFlexBox, extendFlexBox);

  // ================== BPM 包络 + Pg Waveform 包络行 ==================
  juce::FlexBox pgWaveformRowFlexBox;
  pgWaveformRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
  pgWaveformRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
  pgWaveformRowFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;

  juce::FlexBox bpmEnvelopeFlexBox;
  bpmEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  bpmEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  bpmEnvelopeFlexBox.items.add(juce::FlexItem(bpmEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  bpmEnvelopeFlexBox.items.add(juce::FlexItem(bpmEnvControlRow).withFlex(0.5).withMaxHeight(24));
  bpmEnvelopeFlexBox.items.add(juce::FlexItem(bpmEnvelopeCanvas).withFlex(2.0).withMinHeight(80));
  pgWaveformRowFlexBox.items.add(juce::FlexItem(bpmEnvelopeFlexBox).withFlex(1.0f).withMargin({0, 10, 0, 0}));

  juce::FlexBox pgWaveformEnvelopeFlexBox;
  pgWaveformEnvelopeFlexBox.flexDirection = juce::FlexBox::Direction::column;
  pgWaveformEnvelopeFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeLabel).withFlex(0.5).withMaxHeight(18));
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvControlRow).withFlex(0.5).withMaxHeight(24));
  pgWaveformEnvelopeFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeCanvas).withFlex(2.0).withMinHeight(80));
  pgWaveformRowFlexBox.items.add(juce::FlexItem(pgWaveformEnvelopeFlexBox).withFlex(1.5f));

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

  // 为 BPM 包络容器内的控件设置布局：Clear | Random | Load File | Min | Max
  auto bpmControlBounds = bpmEnvControlRow.getLocalBounds();
  int bpmCtrlW = bpmControlBounds.getWidth();
  int bpmLabelW = 32;
  int bpmBtnW = juce::jmax(40, (bpmCtrlW - bpmLabelW * 2) / 5);
  bpmEnvelopeClearButton.setBounds(bpmControlBounds.removeFromLeft(bpmBtnW));
  bpmEnvelopeRandomButton.setBounds(bpmControlBounds.removeFromLeft(bpmBtnW));
  bpmLoadFileButton.setBounds(bpmControlBounds.removeFromLeft(bpmBtnW));
  bpmYMinLabel.setBounds(bpmControlBounds.removeFromLeft(bpmLabelW));
  bpmYMinSlider.setBounds(bpmControlBounds.removeFromLeft(bpmBtnW));
  bpmYMaxLabel.setBounds(bpmControlBounds.removeFromLeft(bpmLabelW));
  bpmYMaxSlider.setBounds(bpmControlBounds);
}

void MainPage::setUIStyle() {
  // output
  outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
  outputGainSlider.setTextValueSuffix(" (db)");
  outputGainLabel.setText("Output", juce::dontSendNotification);

  // bpm 包络：Y轴为合理BPM范围，默认全部120；latch方式推进，每个stage走完推进到下一个点
  bpmEnvelopeLabel.setText("BPM Envelope (Drawn)", juce::dontSendNotification);
  bpmEnvelopeCanvas.setYAxisRange(30.0f, 300.0f);
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultBpmData;
  defaultBpmData.fill(120.0f);
  bpmEnvelopeCanvas.setEnvelopeData(defaultBpmData);

  // Y轴区间控制：同时决定画布显示范围和voice端的bpm限幅区间
  bpmYMinLabel.setText("Min", juce::dontSendNotification);
  bpmYMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  bpmYMinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  bpmYMinSlider.setRange(10.0, 300.0, 1.0);
  bpmYMinSlider.setValue(30.0, juce::dontSendNotification);

  bpmYMaxLabel.setText("Max", juce::dontSendNotification);
  bpmYMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  bpmYMaxSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  bpmYMaxSlider.setRange(10.0, 300.0, 1.0);
  bpmYMaxSlider.setValue(300.0, juce::dontSendNotification);

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
  // attackSlider.setSliderStyle(juce::Slider::LinearVertical);
  // attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  // attackSlider.setTextValueSuffix("");

  // attackLabel.setText("Attack", juce::dontSendNotification);

  // decaySlider.setSliderStyle(juce::Slider::LinearVertical);
  // decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  // decaySlider.setTextValueSuffix("");

  // decayLabel.setText("Decay", juce::dontSendNotification);

  // sustainSlider.setSliderStyle(juce::Slider::LinearVertical);
  // sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  // sustainSlider.setTextValueSuffix("");

  // sustainLabel.setText("Sustain", juce::dontSendNotification);

  // releaseSlider.setSliderStyle(juce::Slider::LinearVertical);
  // releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
  // releaseSlider.setTextValueSuffix("");

  // releaseLabel.setText("Release", juce::dontSendNotification);

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

  // ========================== per-grain ADSR窗控件样式 ==========================
  grainAdsrLabel.setText("ADSR", juce::dontSendNotification);
  auto setupAdsrSlider = [](juce::Slider &s, double defaultVal, const juce::String &tip) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 16);
    s.setRange(0.0, 1.0, 0.01);
    s.setValue(defaultVal, juce::dontSendNotification);
    s.setTooltip(tip);
  };
  setupAdsrSlider(grainAdsrAttackSlider, 0.1, "Attack: ratio of grain lifetime");
  setupAdsrSlider(grainAdsrDecaySlider, 0.2, "Decay: ratio of grain lifetime");
  setupAdsrSlider(grainAdsrSustainSlider, 0.7, "Sustain level");
  setupAdsrSlider(grainAdsrReleaseSlider, 0.4, "Release: ratio of grain lifetime");

  // ========================== pulsar extension控件样式 ==========================
  extendLabel.setText("Extend", juce::dontSendNotification);
  auto setupExtendSlider = [](juce::Slider &s, const juce::String &tip) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 16);
    s.setTooltip(tip);
  };
  setupExtendSlider(harmonicsSlider, "Harmonics: level of coherent 2x/3x/4x harmonic pulsar layers (0 = off)");
  setupExtendSlider(unisonDetuneSlider, "Unison detune: cents of the detuned grain pair (0 = off)");
  setupExtendSlider(unisonWidthSlider, "Unison width: stereo spread of the detuned pair");

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
  // bpm包络：Clear重置为恒定120，Random随机生成；setEnvelopeData/randomize会广播变更，
  // 由PluginEditor的changeListenerCallback统一同步到synth
  bpmEnvelopeClearButton.onClick = [this] {
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultBpmData;
    defaultBpmData.fill(120.0f);
    bpmEnvelopeCanvas.setEnvelopeData(defaultBpmData);
  };
  bpmEnvelopeRandomButton.onClick = [this] { bpmEnvelopeCanvas.randomize(); };

  // Y轴区间变化：保持min<max，同步画布显示范围+现有数据限幅+voice端限幅区间
  bpmYMinSlider.onValueChange = [this] { applyBpmYRange(); };
  bpmYMaxSlider.onValueChange = [this] { applyBpmYRange(); };

  // 从音频文件加载bpm包络：重采样到2048点，按文件自身min/max归一化后映射到当前Y区间
  bpmLoadFileButton.onClick = [this] {
    auto chooser = std::make_shared<juce::FileChooser>("Load audio file as BPM envelope", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser &fc) {
      auto file = fc.getResult();
      if (!file.existsAsFile())
        return;

      juce::AudioFormatManager formatManager;
      formatManager.registerBasicFormats();
      std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
      if (!reader)
        return;

      int64 numSrcSamples = reader->lengthInSamples;
      if (numSrcSamples <= 0)
        return;

      int readSamples = (numSrcSamples > (int64)std::numeric_limits<int>::max() / 2) ? static_cast<int>(std::numeric_limits<int>::max() / 2) : static_cast<int>(numSrcSamples);
      juce::AudioBuffer<float> srcBuffer(1, readSamples);
      reader->read(&srcBuffer, 0, srcBuffer.getNumSamples(), 0, true, false);

      const float *src = srcBuffer.getReadPointer(0);
      int srcLen = srcBuffer.getNumSamples();
      constexpr int targetSize = EnvelopeCanvas::ENVELOPE_SIZE;

      // 重采样到2048点
      std::array<float, targetSize> resampled;
      for (int i = 0; i < targetSize; ++i) {
        float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
        int idx0 = std::min(static_cast<int>(srcIdx), srcLen - 1);
        int idx1 = std::min(idx0 + 1, srcLen - 1);
        float frac = srcIdx - idx0;
        resampled[i] = src[idx0] + frac * (src[idx1] - src[idx0]);
      }

      // 按文件自身幅度归一化到[0,1]，再映射到当前Y区间[min,max]
      float vMin = resampled[0], vMax = resampled[0];
      for (float v : resampled) {
        vMin = std::min(vMin, v);
        vMax = std::max(vMax, v);
      }
      float range = vMax - vMin;
      float yMin = static_cast<float>(bpmYMinSlider.getValue());
      float yMax = static_cast<float>(bpmYMaxSlider.getValue());
      std::array<float, targetSize> bpmData;
      for (int i = 0; i < targetSize; ++i) {
        float norm = range > 1.0e-6f ? (resampled[i] - vMin) / range : 0.5f;
        bpmData[i] = yMin + norm * (yMax - yMin);
      }

      juce::MessageManager::callAsync([this, bpmData] {
        // notify=true：由PluginEditor的changeListenerCallback同步到synth
        bpmEnvelopeCanvas.setEnvelopeData(bpmData);
      });
    });
  };

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
    auto chooser =
        std::make_shared<juce::FileChooser>("Load audio files as wavetable (multi-select)", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");
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

                           // scan all files
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
                             // single file data
                             const float *src = srcBuffer.getReadPointer(0);
                             int srcLen = srcBuffer.getNumSamples();
                             int numFileFrames = CommonVoiceSate::FRAMES_PER_FILE;
                             if (srcLen < targetSize * 2)
                               numFileFrames = 1; // 文件太短，退化为单帧
                             numFileFrames = std::min(numFileFrames, CommonVoiceSate::WAVETABLE_FRAMES - static_cast<int>(frames.size()));

                             // 帧提取用"连续2048样本切片"而不是整段抽稀：之前把可能长达数秒的段落
                             // 线性抽稀到2048点(~100:1无抗混叠降采样)，任何样本都退化成相似的宽带锯齿状抖动
                             // ——这就是不同sample听起来雷同的根源。连续切片保留源素材真实的
                             // 频谱内容(音色/谐波结构)，切片起点沿文件均匀分布，scanning时扫过样本的真实音色轨迹
                             for (int f = 0; f < numFileFrames; ++f) {
                               std::array<float, targetSize> frame;
                               if (srcLen >= targetSize) {
                                 int maxStart = srcLen - targetSize;
                                 int frameStart = numFileFrames > 1 ? static_cast<int>(static_cast<int64>(maxStart) * f / (numFileFrames - 1)) : maxStart / 2;
                                 std::copy(src + frameStart, src + frameStart + targetSize, frame.begin());
                               } else {
                                 // 文件比一帧还短：线性拉伸填满
                                 for (int i = 0; i < targetSize; ++i) {
                                   float srcIdx = static_cast<float>(i) * (srcLen - 1) / (targetSize - 1);
                                   int idx0 = std::min(static_cast<int>(srcIdx), srcLen - 1);
                                   int idx1 = std::min(idx0 + 1, srcLen - 1);
                                   float frac = srcIdx - idx0;
                                   frame[i] = src[idx0] + frac * (src[idx1] - src[idx0]);
                                 }
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

  // per-grain ADSR窗: 任一旋钮变化时把4个值一起同步到synth
  auto syncGrainAdsr = [this] {
    float a = static_cast<float>(grainAdsrAttackSlider.getValue());
    float d = static_cast<float>(grainAdsrDecaySlider.getValue());
    float s = static_cast<float>(grainAdsrSustainSlider.getValue());
    float r = static_cast<float>(grainAdsrReleaseSlider.getValue());
    processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setGrainAdsr(a, d, s, r); });
  };
  grainAdsrAttackSlider.onValueChange = syncGrainAdsr;
  grainAdsrDecaySlider.onValueChange = syncGrainAdsr;
  grainAdsrSustainSlider.onValueChange = syncGrainAdsr;
  grainAdsrReleaseSlider.onValueChange = syncGrainAdsr;

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

// Y轴区间应用：保持min<max（至少相1），更新画布显示范围，现有数据限幅到新区间，
// 并同步voice端的限幅区间；setEnvelopeData会广播，由PluginEditor同步包络数据到synth
void MainPage::applyBpmYRange() {
  double minVal = bpmYMinSlider.getValue();
  double maxVal = bpmYMaxSlider.getValue();
  if (minVal >= maxVal) {
    maxVal = minVal + 1.0;
    bpmYMaxSlider.setValue(maxVal, juce::dontSendNotification);
  }

  float yMin = static_cast<float>(minVal);
  float yMax = static_cast<float>(maxVal);
  bpmEnvelopeCanvas.setYAxisRange(yMin, yMax);

  // 现有包络数据限幅到新区间，避免数据超出显示范围
  auto data = bpmEnvelopeCanvas.getEnvelopeData();
  for (float &v : data)
    v = juce::jlimit(yMin, yMax, v);
  bpmEnvelopeCanvas.setEnvelopeData(data);

  processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->setBpmEnvelopeYRange(yMin, yMax); });
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
