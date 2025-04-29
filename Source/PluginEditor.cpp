#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryResourceSingleton.h"

//===================================核心逻辑 START===========================================

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused(processorRef);

    //register the editor as a listener
    processorRef.addChangeListener(this);

    //init window size
    setWindowSize();

    //set ui element visible
    makeVisible();

    //ui element style
    setUIStyle();

    //设置attachment，保证ui element和parameter同步
    connectUIAndAudioParameter();

    //trigger event
    initUITriggerEvent();

    //设置初始值：上一次窗口打开的值
    setLastValueAfterCloseWindow();

    //The listening of apvts is placed at the end to ensure that the modifications will not be overwritten
    //by the previous logic. When DAW opens the project, it may call setStateInformation multiple times,
    //but plugineditor has not been generated yet, resulting in the broadcast changelistener listening function
    //not being executed So the loading preset tag will eventually end only after the editor
    //(when the plugin window is opened) is updated, ensuring that the UI display is up to date
    if (processorRef.isLoadingPresetFlag())
    {
        changeListenerCallback(&processorRef);
    }
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    //release
    processorRef.removeChangeListener(this);
}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // g.setColour(juce::Colours::white);
    // g.setFont(15.0f);
    // g.drawFittedText("Hello Mr. Wang! When will you return to the distant planet",
    // getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::setLastValueAfterCloseWindow()
{
    juce::String currentStochasticMaskStr = juce::String(processorRef.getPulsarSynthEngine().
                                                                      getCurrentPulsarSynth()->
                                                                      getStochasticMaskStr());
    if (stochasticMaskTextEditor.getText() != currentStochasticMaskStr)
    {
        //property将会被存为state information，用来恢复参数
        processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
        //展示最新stochastic mask
        stochasticMaskTextEditor.setText(currentStochasticMaskStr);
    }
    if (!processorRef.apvts.state.getProperty(why::PropertyID::burstMask).isVoid())
    {
        burstMaskTextEditor.setText(processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString());
    }
    if (!processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).isVoid())
    {
        sampleImpulsePathTextEditor.setText(
            processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString());
    }
}

/**
 * set ui element display
 */
void AudioPluginAudioProcessorEditor::resized()
{
    // 获取当前窗口的区域
    auto area = getLocalBounds();

    juce::FlexBox mainFlexBox;
    mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

    //top
    juce::FlexBox topFlexBox;
    std::shared_ptr<juce::FlexBox> row1 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row2 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row3 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row4 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row5 = std::make_shared<juce::FlexBox>();

    this->topFlexBox(topFlexBox, row1, row2, row3, row4, row5);

    //midlle part
    juce::FlexBox midFlexBox;
    std::shared_ptr<juce::FlexBox> row11 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row12 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row13 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row14 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row15 = std::make_shared<juce::FlexBox>();

    this->midFlexBox(midFlexBox, row11, row12, row13, row14, row15);

    //bottom
    juce::FlexBox bottomFlexBox;
    std::shared_ptr<juce::FlexBox> column1 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column2 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column3 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column4 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column5 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column6 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column7 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column8 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> column9 = std::make_shared<juce::FlexBox>();

    this->bottomFlexBox(bottomFlexBox, column1, column2, column3, column4, column5, column6, column7, column8, column9);

    //Overall combination, withMargin: up, right, down, left
    mainFlexBox.items.add(juce::FlexItem(topFlexBox).withFlex(1.0).withMargin({20, 20, 0, 20}));
    mainFlexBox.items.add(juce::FlexItem(midFlexBox).withFlex(0.8).withMargin({20, 20, 0, 20}));
    mainFlexBox.items.add(juce::FlexItem(bottomFlexBox).withFlex(1.0).withMargin({20, 20, 20, 20}));
    mainFlexBox.performLayout(area);
}

/**
 * Mainly handle UI updates where attachment cannot be set, such as the loading logic of texteditor and impulse file
 */
void AudioPluginAudioProcessorEditor::refreshUIFromPreset()
{
    if (!processorRef.isLoadingPresetFlag())
    {
        return;
    }
    //Refresh the texteditor display
    String burstMask = processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString();
    String stochasticMask = processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString();
    String sampleImpulsePath = processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString();
    //保证展示不为空
    if (burstMaskTextEditor.getTextValue() != burstMask)
    {
        burstMaskTextEditor.setText(burstMask, juce::dontSendNotification);
    }
    if (stochasticMaskTextEditor.getTextValue() != stochasticMask)
    {
        stochasticMaskTextEditor.setText(stochasticMask, juce::dontSendNotification);
    }

    //display the lastest sample impulse file path
    if (sampleImpulsePath.isNotEmpty())
    {
        sampleImpulsePathTextEditor.setText(sampleImpulsePath, juce::dontSendNotification);
        juce::File file(sampleImpulsePath);
        if (!file.existsAsFile())
        {
            sampleImpulsePathTextEditor.setText("File does not exist: " + sampleImpulsePath,
                                                juce::dontSendNotification);
        }
    }
}

/**
 * invoke listener callback
 * @param source listener
 */
void AudioPluginAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    //reload preset：It will execute up to here only after the plugin window is opened
    if (source == &processorRef && processorRef.isLoadingPresetFlag())
    {
        refreshUIFromPreset();
        processorRef.setLoadingPresetFlag(false);
        return;
    }

    //It is not triggered by reload preset but others like by parameterChanged
    if (source == &processorRef)
    {
        //当train参数改变后，随机mask展示要刷新
        juce::String currentStochasticMaskStr = juce::String(processorRef.getPulsarSynthEngine().
                                                                          getCurrentPulsarSynth()->
                                                                          getStochasticMaskStr());
        if (stochasticMaskTextEditor.getText() != currentStochasticMaskStr)
        {
            //property将会被存为state information，用来恢复参数
            processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
            //展示最新stochastic mask
            stochasticMaskTextEditor.setText(currentStochasticMaskStr);
        }
    }
}

//===================================核心逻辑 END===========================================
/**
 * Head layout: Arrange each flexbox vertically (internal horizontal layout)
 * @param flexBoxTop 头部布局的flexbox
 * @param trainLenFlexBox  train len flexbox in a row
 * @param trainDutyCycleFlexBox train duty cyle flexbox in a row
 * @param trainSilenceLenFlexBox train silence length flexbox in a row
 * @param bpmFlexBox bpm flexbox in a row
 * @param playModeAndImpulseFlexBox  a flexbox includes play mode and impulse file ui elements in a row
 */
void AudioPluginAudioProcessorEditor::topFlexBox(juce::FlexBox& flexBoxTop, std::shared_ptr<FlexBox> trainLenFlexBox,
                                                 std::shared_ptr<FlexBox> trainDutyCycleFlexBox,
                                                 std::shared_ptr<FlexBox> trainSilenceLenFlexBox,
                                                 std::shared_ptr<FlexBox> bpmFlexBox,
                                                 std::shared_ptr<FlexBox> playModeAndImpulseFlexBox
)
{
    flexBoxTop.flexDirection = juce::FlexBox::Direction::column;
    flexBoxTop.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    flexBoxTop.alignContent = juce::FlexBox::AlignContent::stretch; //在wrap换行时才有用，决定换行元素的排列
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

    playModeAndImpulseFlexBox->flexDirection = juce::FlexBox::Direction::row;
    playModeAndImpulseFlexBox->justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    playModeAndImpulseFlexBox->items.
                               add(juce::FlexItem(playModeLabel).withFlex(0.5).withMaxWidth(50).withMaxHeight(40));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(playModeCombobox).withFlex(0.5).withMaxWidth(70).withMaxHeight(40));

    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(impulseSwitchLabel).withFlex(0.5).withMaxWidth(70).withMaxHeight(40));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(impulseSwitchComboBox).withFlex(1).withMaxWidth(90).withMaxHeight(40));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(impulseTemplateLabel).withFlex(0.5).withMaxWidth(60).withMaxHeight(40));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(impulseTemplateFileComboBox).withFlex(1.0).withMaxWidth(150).withMaxHeight(40));
    // row5->items.add(juce::FlexItem().withFlex(0.2));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(selectSampleImpulseFileButton).withFlex(0.5).withMaxWidth(100).withMaxHeight(40));
    playModeAndImpulseFlexBox->items.add(
        juce::FlexItem(sampleImpulsePathTextEditor).withFlex(1).withMaxWidth(500).withMaxHeight(50));
    flexBoxTop.items.add(juce::FlexItem(*playModeAndImpulseFlexBox).withFlex(1.5f));
}

/**
 * Arrange each flexbox horizontally (with vertical arrangement inside)
 * @param bottomFlexBox bottom flexbox
 * @param pulsarWaveformFlexBox pulsar wave form in row
 * @param pulsarDutyCycleClusterLenFlexBox pulsar duty cycle cluster length in a row
 * @param pulsarDutyCycleRatioFlexBox pulsar duty cyle ratio in a row
 * @param ampLfoFlexBox amp lfo in a row
 * @param formantFreqLfoFlexBox formant frequency lfo in a row
 * @param attackFlexBox attack in a row
 * @param decayFlexBox decay in a row
 * @param sustainFlexBox sustain in a row
 * @param releaseFlexBox release in a row
 */
void AudioPluginAudioProcessorEditor::bottomFlexBox(juce::FlexBox& bottomFlexBox,
                                                    std::shared_ptr<juce::FlexBox> pulsarWaveformFlexBox,
                                                    std::shared_ptr<juce::FlexBox> pulsarDutyCycleClusterLenFlexBox,
                                                    std::shared_ptr<juce::FlexBox> pulsarDutyCycleRatioFlexBox,
                                                    std::shared_ptr<juce::FlexBox> ampLfoFlexBox,
                                                    std::shared_ptr<juce::FlexBox> formantFreqLfoFlexBox,
                                                    std::shared_ptr<juce::FlexBox> attackFlexBox,
                                                    std::shared_ptr<juce::FlexBox> decayFlexBox,
                                                    std::shared_ptr<juce::FlexBox> sustainFlexBox,
                                                    std::shared_ptr<juce::FlexBox> releaseFlexBox)
{
    bottomFlexBox.flexDirection = juce::FlexBox::Direction::row;
    // flexBoxLeftBottom.flexWrap = juce::FlexBox::Wrap::noWrap; // 是否换行
    bottomFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    bottomFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart; //

    pulsarWaveformFlexBox->flexDirection = juce::FlexBox::Direction::column;
    pulsarWaveformFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    pulsarWaveformFlexBox->items.add(
        juce::FlexItem(pulsarWaveformLabel).withFlex(1.0).withMaxWidth(200).withMaxHeight(20));
    pulsarWaveformFlexBox->items.add(juce::FlexItem(pulsarWaveformSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*pulsarWaveformFlexBox).withFlex(1.0f));

    pulsarDutyCycleClusterLenFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    pulsarDutyCycleClusterLenFlexBox->flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    pulsarDutyCycleClusterLenFlexBox->items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    pulsarDutyCycleClusterLenFlexBox->items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*pulsarDutyCycleClusterLenFlexBox).withFlex(1.0f));

    pulsarDutyCycleRatioFlexBox->flexDirection = juce::FlexBox::Direction::column;
    pulsarDutyCycleRatioFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    pulsarDutyCycleRatioFlexBox->items.add(
        juce::FlexItem(pulsarDutyCycleRatioLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    pulsarDutyCycleRatioFlexBox->items.add(
        juce::FlexItem(pulsarDutyCycleRatioSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*pulsarDutyCycleRatioFlexBox).withFlex(1.0f));

    //lfo modulation
    ampLfoFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    ampLfoFlexBox->flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    ampLfoFlexBox->items.add(juce::FlexItem(ampLfoLabel).withFlex(1).withMaxHeight(20));
    ampLfoFlexBox->items.add(
        juce::FlexItem(ampLfoSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*ampLfoFlexBox).withFlex(1.0f));

    formantFreqLfoFlexBox->flexDirection = juce::FlexBox::Direction::column;
    formantFreqLfoFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    formantFreqLfoFlexBox->items.add(
        juce::FlexItem(formantFreqLfoLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    formantFreqLfoFlexBox->items.add(
        juce::FlexItem(formantFreqLfoSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*formantFreqLfoFlexBox).withFlex(1.0f));


    //envelope
    attackFlexBox->flexDirection = juce::FlexBox::Direction::column;
    attackFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    attackFlexBox->items.add(
        juce::FlexItem(attackLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    attackFlexBox->items.add(
        juce::FlexItem(attackSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*attackFlexBox).withFlex(1.0f));

    decayFlexBox->flexDirection = juce::FlexBox::Direction::column;
    decayFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    decayFlexBox->items.add(
        juce::FlexItem(decayLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    decayFlexBox->items.add(
        juce::FlexItem(decaySlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*decayFlexBox).withFlex(1.0f));

    sustainFlexBox->flexDirection = juce::FlexBox::Direction::column;
    sustainFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    sustainFlexBox->items.add(
        juce::FlexItem(sustainLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    sustainFlexBox->items.add(
        juce::FlexItem(sustainSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*sustainFlexBox).withFlex(1.0f));

    releaseFlexBox->flexDirection = juce::FlexBox::Direction::column;
    releaseFlexBox->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    releaseFlexBox->items.add(
        juce::FlexItem(releaseLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    releaseFlexBox->items.add(
        juce::FlexItem(releaseSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*releaseFlexBox).withFlex(1.0f));

    // 调整控件大小
    // flexBoxLeftBottom.performLayout(area);
}

/**
 * Central flexbox layout: Arrange each flexbox horizontally (internal horizontal layout)
 * @param midFlexBox 中部布局的flexbox
 * @param maskOptionFlexBox mask option in a row
 * @param burstMaskFlexBox burst mask in a row
 * @param euclidStepFlexBox euclid step in a row
 * @param euclidHitFlexBox euclid hit in a row
 * @param stochasticMaskFlexBox stochastic mask in a row
 */
void AudioPluginAudioProcessorEditor::midFlexBox(juce::FlexBox& midFlexBox,
                                                 std::shared_ptr<juce::FlexBox> maskOptionFlexBox,
                                                 std::shared_ptr<juce::FlexBox> burstMaskFlexBox,
                                                 std::shared_ptr<juce::FlexBox> euclidStepFlexBox,
                                                 std::shared_ptr<juce::FlexBox> euclidHitFlexBox,
                                                 std::shared_ptr<juce::FlexBox> stochasticMaskFlexBox)
{
    midFlexBox.flexDirection = juce::FlexBox::Direction::row; // 水平排列（三个Slider）

    maskOptionFlexBox->flexDirection = juce::FlexBox::Direction::row;
    maskOptionFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    maskOptionFlexBox->items.add(juce::FlexItem(maskComboBoxLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    maskOptionFlexBox->items.add(juce::FlexItem(maskOptionComboBox).withFlex(1.0).withMaxHeight(20));
    midFlexBox.items.add(juce::FlexItem(*maskOptionFlexBox).withFlex(1.0f));

    burstMaskFlexBox->flexDirection = juce::FlexBox::Direction::row;
    burstMaskFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    burstMaskFlexBox->items.add(juce::FlexItem(burstMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    burstMaskFlexBox->items.add(
        juce::FlexItem(burstMaskTextEditor).withFlex(1.0).withMinWidth(100).withMaxWidth(250).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*burstMaskFlexBox).withFlex(1.5f));

    euclidStepFlexBox->flexDirection = juce::FlexBox::Direction::row;
    // flexBoxTop->justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    euclidStepFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    euclidStepFlexBox->items.add(juce::FlexItem(euclidStepLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    euclidStepFlexBox->items.add(
        juce::FlexItem(euclidStepSlider).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*euclidStepFlexBox).withFlex(0.9f));

    euclidHitFlexBox->flexDirection = juce::FlexBox::Direction::row;
    euclidHitFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    euclidHitFlexBox->items.add(juce::FlexItem(euclidHitLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    euclidHitFlexBox->items.add(juce::FlexItem(euclidHitSlider).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*euclidHitFlexBox).withFlex(0.9f));

    stochasticMaskFlexBox->flexDirection = juce::FlexBox::Direction::row;
    stochasticMaskFlexBox->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    stochasticMaskFlexBox->items.add(
        juce::FlexItem(stochasticMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    stochasticMaskFlexBox->items.add(
        juce::FlexItem(stochasticMaskTextEditor).withFlex(1.0).withMaxWidth(250).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*stochasticMaskFlexBox).withFlex(1.5f));

    // flexBoxMiddle.performLayout(area.removeFromTop(150));
    //
    // area.removeFromTop(20); // 插入 20px 空白
}

/**
 * set ui element visible
 */
void AudioPluginAudioProcessorEditor::makeVisible()
{
    //output gain
    addAndMakeVisible(outputGainLabel);
    addAndMakeVisible(outputGainSlider);

    //play mode
    addAndMakeVisible(playModeLabel);
    addAndMakeVisible(playModeCombobox);

    //bpm
    addAndMakeVisible(bpmSlider);

    //train
    addAndMakeVisible(trainLenSlider);
    addAndMakeVisible(trainDutyCycleLenSlider);
    addAndMakeVisible(trainSilenceLenSlider);

    addAndMakeVisible(trainLenLabel);
    addAndMakeVisible(trainDutyCycleLenLabel);
    addAndMakeVisible(trainSilenceLenLabel);

    //pulsar
    addAndMakeVisible(pulsarWaveformSlider);
    addAndMakeVisible(pulsarDutyCycleRatioSlider);
    addAndMakeVisible(pulsarDutyCycleClusterLenSlider);

    addAndMakeVisible(pulsarWaveformLabel);
    addAndMakeVisible(pulsarDutyCycleRatioLabel);
    addAndMakeVisible(pulsarDutyCycleClusterLenLabel);

    //lfo
    addAndMakeVisible(ampLfoSlider);
    addAndMakeVisible(formantFreqLfoSlider);

    addAndMakeVisible(ampLfoLabel);
    addAndMakeVisible(formantFreqLfoLabel);

    //envelope
    addAndMakeVisible(attackSlider);
    addAndMakeVisible(decaySlider);
    addAndMakeVisible(sustainSlider);
    addAndMakeVisible(releaseSlider);

    addAndMakeVisible(attackLabel);
    addAndMakeVisible(decayLabel);
    addAndMakeVisible(sustainLabel);
    addAndMakeVisible(releaseLabel);

    //mask
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

    //convolution impulse
    addAndMakeVisible(selectSampleImpulseFileButton);
    addAndMakeVisible(impulseTemplateLabel);
    addAndMakeVisible(impulseTemplateFileComboBox);
    addAndMakeVisible(impulseSwitchComboBox);
    addAndMakeVisible(sampleImpulsePathTextEditor);
    addAndMakeVisible(impulseSwitchLabel);
}

/**
* set ui style
*/
void AudioPluginAudioProcessorEditor::setUIStyle()
{
    //output
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    outputGainSlider.setTextValueSuffix(" (db)");

    outputGainLabel.setText("Output", juce::dontSendNotification);

    //play mode
    playModeCombobox.addItem("Off", static_cast<int>(why::PlayModeEnum::NotSelected) + 1);
    playModeCombobox.addItem("Auto", static_cast<int>(why::PlayModeEnum::Auto) + 1);
    //item id不能为0，但是parameter获取到的值是从0开始
    playModeCombobox.addItem("Midi", static_cast<int>(why::PlayModeEnum::Midi) + 1);
    //setSelectId也会触发event回调，所以重新打开插件窗口，会触发event事件，没必要，因为该combobox绑定了attachment，会自动更新为最新
    // playModeCombobox.setSelectedId(1);

    playModeLabel.setText("Trigger", juce::dontSendNotification);

    //bpm
    bpmSlider.setRange(30.0, 300.0, 1.0); // 合理BPM范围
    bpmSlider.setTextValueSuffix(" BPM");
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal); // 或 Rotary
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    //train
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
    // trainSilenceLenLabel.attachToComponent(&trainSilenceLenSlider, true);

    //pulsar
    pulsarWaveformSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulsarWaveformSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    pulsarWaveformSlider.setTextValueSuffix("");
    pulsarWaveformSlider.setRange(0.0, 1.0, 0.001);

    pulsarWaveformLabel.setText("Pulsar Waveform", juce::dontSendNotification);

    pulsarDutyCycleClusterLenSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulsarDutyCycleClusterLenSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    pulsarDutyCycleClusterLenSlider.setTextValueSuffix("");

    pulsarDutyCycleClusterLenLabel.setText("Pulsar Duty Cycle Cluster", juce::dontSendNotification);


    pulsarDutyCycleRatioSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulsarDutyCycleRatioSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    pulsarDutyCycleRatioSlider.setTextValueSuffix("");

    pulsarDutyCycleRatioLabel.setText("Pulsar Duty Cycle Ratio", juce::dontSendNotification);

    //lfo
    ampLfoSlider.setSliderStyle(juce::Slider::LinearVertical);
    ampLfoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    ampLfoSlider.setTextValueSuffix("");
    ampLfoSlider.setRange(0.0, 1.0, 0.001);

    ampLfoLabel.setText("AM Waveform", juce::dontSendNotification);


    formantFreqLfoSlider.setSliderStyle(juce::Slider::LinearVertical);
    formantFreqLfoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    formantFreqLfoSlider.setTextValueSuffix("");
    formantFreqLfoSlider.setRange(0.0, 1.0, 0.001);

    formantFreqLfoLabel.setText("FM Waveform", juce::dontSendNotification);

    //envelope
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

    //masking
    maskComboBoxLabel.setText("Mask Mode", juce::dontSendNotification);
    maskOptionComboBox.addItem("Off", static_cast<int>(why::MaskOptionEnum::Off) + 1);
    //item id不能为0，但是parameter获取到的值是从0开始
    maskOptionComboBox.addItem("Burst Mask", static_cast<int>(why::MaskOptionEnum::BurstMask) + 1);
    maskOptionComboBox.addItem("Euclid Mask", static_cast<int>(why::MaskOptionEnum::EuclidMask) + 1);
    maskOptionComboBox.addItem("Stochastic Mask", static_cast<int>(why::MaskOptionEnum::StochasticMask) + 1);
    // maskOptionComboBox.setSelectedId(1);

    impulseTemplateLabel.setText("Template", juce::dontSendNotification);
    // impulseTemplateFileComboBox.setSelectedId(1);

    burstMaskLabel.setText("Burst Mask", juce::dontSendNotification);

    burstMaskTextEditor.setMultiLine(true);
    burstMaskTextEditor.setScrollbarsShown(true);
    burstMaskTextEditor.setPopupMenuEnabled(false);
    burstMaskTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
    burstMaskTextEditor.setInputRestrictions(256, "01"); //限制只输入0或1
    burstMaskTextEditor.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(
                                           256, // 最长 256 个字符（你可以改）
                                           "01" // 只允许字符 '0' 和 '1'
                                       ), true); // true 表示替换当前 filter
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
    stochasticMaskTextEditor.setInputRestrictions(0, "01"); //限制只输入0或1
    stochasticMaskTextEditor.setTextToShowWhenEmpty("Generated mask will be here", juce::Colours::grey);

    //卷积
    // 设置按钮文本
    selectSampleImpulseFileButton.setButtonText("Select Impulse");

    impulseSwitchComboBox.addItem("Off", static_cast<int>(why::ImpulseSwitchEnum::Off) + 1);
    impulseSwitchComboBox.addItem("Template Impluse", static_cast<int>(why::ImpulseSwitchEnum::Template) + 1);
    impulseSwitchComboBox.addItem("Sample Impulse", static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1);
    // impulseSwitchComboBox.setSelectedId(1);

    impulseSwitchLabel.setText("Impulse", juce::dontSendNotification);

    //初始化下拉框列表，必须放到设置attachment否则不会关联上
    initTemplateImpulseComboboxNames();

    sampleImpulsePathTextEditor.setMultiLine(true);
    sampleImpulsePathTextEditor.setReadOnly(true);
    sampleImpulsePathTextEditor.setScrollbarsShown(true);
    sampleImpulsePathTextEditor.setPopupMenuEnabled(false);
    sampleImpulsePathTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
    sampleImpulsePathTextEditor.setInputRestrictions(0, "01"); //限制只输入0或1
    sampleImpulsePathTextEditor.setTextToShowWhenEmpty("No selected file", juce::Colours::grey);
}

/**
 * Set the attachment to bind the ui element to the audio parameter, thereby ensuring that
 * the latest value of the parameter is synchronized to the ui, such as automation
 */
void AudioPluginAudioProcessorEditor::connectUIAndAudioParameter()
{
    //绑定UI与Parameter
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::outputGain, outputGainSlider);

    playModeComboboxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::playMode, playModeCombobox);

    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::bpm, bpmSlider);

    //train
    trainLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::trainLen, trainLenSlider);
    trainDutyCycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::trainDutyCycleLen, trainDutyCycleLenSlider);
    trainSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::trainSilenceLen, trainSilenceLenSlider);

    //pulsar
    pulsarWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarWaveform, pulsarWaveformSlider);
    pulsarDutyCycleClusterLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarDutyCycleClusterLen, pulsarDutyCycleClusterLenSlider);
    pulsarDutyCycleRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarDutyCycleRatio, pulsarDutyCycleRatioSlider);

    ampLfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::ampLfoWaveform, ampLfoSlider);
    formantFreqLfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::formantFreqLfoWaveform, formantFreqLfoSlider);

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarAttack, attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarDecay, decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarSustain, sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::pulsarRelease, releaseSlider);

    maskOptionComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::maskOption, maskOptionComboBox);
    euclidStepDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::euclidSteps, euclidStepSlider);
    euclidHitDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::euclidHits, euclidHitSlider);

    //impulse file
    impulseSwitchComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::impulseSwitch, impulseSwitchComboBox);
    impulseTemplateFileComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::impulseTemplateFile, impulseTemplateFileComboBox);
}

/**
 * Settings of all ui element event callback methods
 */
void AudioPluginAudioProcessorEditor::initUITriggerEvent()
{
    //sample impulse文件选择，点击事件
    selectSampleImpulseFileButton.onClick = [this]
    {
        openFileChooser();
    };

    //burst mask text editor回车，没有attachment，需要手动更新synth状态
    burstMaskTextEditor.onReturnKey = [&]
    {
        juce::String currentBurstMaskText = burstMaskTextEditor.getText(); // 获取编辑框中的文本

        //刷新synth的burst mask标记
        processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->refreshBurstMask(currentBurstMaskText);
        });

        //保存到property，在load preset时，可从parameterChanged监听中获得property值，从而恢复状态
        processorRef.apvts.state.setProperty(why::PropertyID::burstMask, currentBurstMaskText, nullptr);

        //color effect
        juce::Colour originalColour = burstMaskTextEditor.findColour(juce::TextEditor::backgroundColourId);
        //Temporary highlighting
        burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::mediumaquamarine);
        burstMaskTextEditor.repaint();
        //recovery
        juce::Timer::callAfterDelay(300, [&, originalColour]()
        {
            burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, originalColour);
            burstMaskTextEditor.repaint();
        });
    };

    //euclid联动设置：始终保持hit<=step
    euclidStepSlider.onValueChange = [&]
    {
        rebalanceStepHitValueDisplay();
    };
    euclidHitSlider.onValueChange = [&]
    {
        rebalanceStepHitValueDisplay();
    };

    //template impulse file下拉框
    impulseTemplateFileComboBox.onChange = [&]
    {
        saveTemplateImpulseThenLoadAfterSelect(impulseTemplateFileComboBox.getSelectedId());
    };

    //reload preset时，如果数据变化也会体现在event
    impulseSwitchComboBox.onChange = [&]
    {
        if (impulseSwitchComboBox.getSelectedId() == static_cast<int>(why::ImpulseSwitchEnum::Template) + 1)
        {
            saveTemplateImpulseThenLoadAfterSelect(impulseTemplateFileComboBox.getSelectedId());
        }
        if (impulseSwitchComboBox.getSelectedId() == static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1)
        {
            // There are two places where sample resources can be loaded:
            // 1. Manually select files.
            // 2. When the load preset is used, the broadcast listener is triggered.
            // Here, you can directly load the impulse response because in both of the above cases,
            // it is guaranteed that the impulse file has been saved to synth
            loadSampleImpulseWhenSelected();
        }
    };

    playModeCombobox.onChange = [&]
    {
        processorRef.getPulsarSynthEngine().setCurrentPlayModeEnum(playModeCombobox.getSelectedItemIndex());
    };

    //强制更新synth依赖的bpm
    bpmSlider.onValueChange = [&]
    {
        processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->forceRefreshBpmAndRebuildTrain(bpmSlider.getValue());
        });
    };

    maskOptionComboBox.onChange = [&]
    {
        //只在选中stochastic mask时才展示生成的随机mask
        if (maskOptionComboBox.getSelectedItemIndex() == static_cast<int>(why::MaskOptionEnum::StochasticMask))
        {
            //stochastic mask默认采用第一个voice的
            std::string maskStrStd = processorRef.getPulsarSynthEngine().getCurrentPulsarSynth()->
                                                  getStochasticMaskStr();
            juce::String stochasticMaskStr = juce::String(maskStrStd);
            stochasticMaskTextEditor.setText(stochasticMaskStr, juce::dontSendNotification);
        }
    };
}

/**
 * euclid linkage setting: When the euclid step changes, the value range of euclid hit is automatically adjusted
 */
void AudioPluginAudioProcessorEditor::rebalanceStepHitValueDisplay()
{
    double stepValue = euclidStepSlider.getValue();
    double hitValue = euclidHitSlider.getValue();

    // 更新 sliderB 的最大值
    if (stepValue <= 1)
    {
        //juce的逻辑：range不可以设置为1，1，必须满足max>min
        //所以step为1，hit不设为1了，直接禁用
        euclidHitSlider.setValue(1);
        euclidHitSlider.setEnabled(false);
    }
    else
    {
        euclidHitSlider.setEnabled(true);
        euclidHitSlider.setRange(euclidHitSlider.getMinimum(), stepValue);
    }

    if (hitValue > stepValue)
        euclidHitSlider.setValue(stepValue, juce::dontSendNotification); // 避免无限触发
}

/**
 * 设置窗口大小
 */
void AudioPluginAudioProcessorEditor::setWindowSize()
{
    // Make sure that before the constructor has finished, you've set the editor's size to whatever you need it to be.
    // setSize(1200, 600);
    // 允许窗口被用户调整大小
    setResizable(true, true);
    // 设置最小和最大尺寸
    setResizeLimits(1024, 600, 1920, 1080);
}

/**
 * Open the file selection window, read and save the selected file
 */
void AudioPluginAudioProcessorEditor::openFileChooser()
{
    // 创建一个文件选择器
    fileChooser = std::make_unique<juce::FileChooser>("Select a file to upload",
                                                      juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                      "*.wav;*.mp3");

    //async read file
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& chooser)
                             {
                                 juce::File selectedFile = chooser.getResult();
                                 if (selectedFile.exists())
                                 {
                                     // juce::Logger::writeToLog("File selected: " + selectedFile.getFullPathName());
                                     saveFileIntoSynth(selectedFile);
                                     //展示路径
                                     sampleImpulsePathTextEditor.setText(selectedFile.getFullPathName());

                                     //保存到property，在load preset时，可从parameterChanged监听中获得property值，进行手动监听
                                     processorRef.apvts.state.setProperty(why::PropertyID::sampleImpulsePath,
                                                                          selectedFile.getFullPathName(), nullptr);

                                     //当前如果是采样模式则直接加载
                                     loadSampleImpulseWhenSelected();
                                 }
                             });
}

/**
 * 保存impulse file到synth的convolution resource中
 * @param file
 */
void AudioPluginAudioProcessorEditor::saveFileIntoSynth(const juce::File& file)
{
    processorRef.getPulsarSynthEngine().getConvolutionResource()->saveLastSampleFileAsBlock(file);
}

/**
 * 当impulse选项选中sample impulse时，load sample impulse file as a impulse response
 */
void AudioPluginAudioProcessorEditor::loadSampleImpulseWhenSelected()
{
    if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1)
    {
        return;
    }
    processorRef.getPulsarSynthEngine().getConvolutionResource()->loadSampleImpulseFile();
}

/**
 * 当impulse选中template impulse时，直接将保存的template impulse加载为impulse response
 */
void AudioPluginAudioProcessorEditor::loadTemplateImpulseWhenSelected()
{
    //当前如果是template模式则直接加载
    if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Template) + 1)
    {
        return;
    }
    processorRef.getPulsarSynthEngine().getConvolutionResource()->loadTemplateImpulseFile();
}

/**
 * Save the binary file under Resources to synth. If impulse selects the template file, it will be loaded as
 * impulse response
*/
void AudioPluginAudioProcessorEditor::saveTemplateImpulseThenLoadAfterSelect(int selectedId)
{
    double fileSampleRate;
    std::unique_ptr<juce::AudioBuffer<float>> bf;

    if (BinaryResourceSingleton::getInstance().readFileFromResources(why::resourceIdToName[selectedId].toRawUTF8(),
                                                                     fileSampleRate, bf))
    {
        processorRef.getPulsarSynthEngine().getConvolutionResource()->saveLastTemplateImpulseData(
            *bf, fileSampleRate, why::resourceIdToName[selectedId].toRawUTF8());
        loadTemplateImpulseWhenSelected();
    }
}

/**
 * Initialize the copy displayed in the template file combobox: that is, the list of file names of Resources
 */
void AudioPluginAudioProcessorEditor::initTemplateImpulseComboboxNames()
{
    // impulseTemplateFileComboBox.removeAllChildren();
    // 塞入 BinaryData 中所有资源文件名
    for (std::pair<const int, juce::String>& pair : why::resourceIdToName)
    {
        impulseTemplateFileComboBox.addItem(pair.second, pair.first);
        // ID 从 1 开始,apvts返回的parametervalue是从0开始
    }

    // 可选：设置默认选择项
    impulseTemplateFileComboBox.setSelectedId(1, juce::dontSendNotification);
}
