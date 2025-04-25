#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

#include "ConvolutionResource.h"
#include "ResourceSingleton.h"

//===================================核心逻辑 START===========================================

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused(processorRef);

    //在该对象上监听change广播
    processorRef.addChangeListener(this);

    //value tree监听
    processorRef.apvts.state.addListener(this);

    //apvts的监听
    //DAW打开工程时，可能会多次调用setStateInformation，但plugineditor还没生成，导致广播changelistener监听函数没执行
    //如果标记还是loading preset则继续处理完，直到打开插件窗口才会初始化plugineditor
    if (processorRef.isLoadingPresetFlag())
    {
        changeListenerCallback(&processorRef);
    }

    //将parameter绑定到当前对象，监听参数变化
    // bindParameterListener();

    //设置总窗口大小
    setWindowSize();

    //设置所有控件可见
    makeVisible();

    //设置控件
    setUIStyle();

    //设置attachment，保证ui element和parameter同步
    connectUIAndAudioParameter();

    //触发事件
    initUITriggerEvent();

    //DAW关闭窗口后重新打开，texteditor的值没展示
    // 从 APVTS 中恢复值
    if (!processorRef.apvts.state.getProperty(why::PropertyID::burstMask).isVoid())
    {
        //get texteditor value from property because juce can't bind texteditor with parameter automatally
        burstMaskTextEditor.setText(
            processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString().toStdString());
    }
    if (!processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).isVoid())
    {
        //get texteditor value from property because juce can't bind texteditor with parameter automatally
        stochasticMaskTextEditor.setText(
            processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString().toStdString());
    }
    if (!processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).isVoid())
    {
        //get texteditor value from property because juce can't bind texteditor with parameter automatally
        sampleImpulsePathTextEditor.setText(
            processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString().toStdString());
    }
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    //在该对象上监听change广播
    processorRef.removeChangeListener(this);

    //value tree监听
    processorRef.apvts.state.removeListener(this);

    // stopTimer(); // 停止定时器
    //移除apvts的监听绑定
    // for (int i = 0; i < processorRef.apvts.state.getNumChildren(); ++i)
    // {
    //     auto child = processorRef.apvts.state.getChild(i);
    //     if (child.hasType("PARAM") && child.hasProperty("id"))
    //     {
    //         juce::String paramID = child["id"];
    //         processorRef.apvts.removeParameterListener(paramID, this);
    //     }
    // }
}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // g.setColour(juce::Colours::white);
    // g.setFont(15.0f);
    // g.drawFittedText("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any subcomponents in your editor..
    // visualiser.setBounds(getLocalBounds());

    // 获取当前窗口的区域
    auto area = getLocalBounds();

    juce::FlexBox mainFlexBox;
    mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

    //头部布局
    juce::FlexBox topFlexBox;
    // Creates an item that represents an embedded FlexBox, will not create a copy of the supplied flex box.
    // ensure the life-time of flexBoxToControl is longer than the FlexItem.
    std::shared_ptr<juce::FlexBox> row1 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row2 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row3 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row4 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row5 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row6 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row7 = std::make_shared<juce::FlexBox>();

    this->topFlexBox(topFlexBox, row1, row2, row3, row4, row5, row6, row7);

    //中部布局
    juce::FlexBox midFlexBox;
    std::shared_ptr<juce::FlexBox> row11 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row12 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row13 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row14 = std::make_shared<juce::FlexBox>();
    std::shared_ptr<juce::FlexBox> row15 = std::make_shared<juce::FlexBox>();

    this->midFlexBox(midFlexBox, row11, row12, row13, row14, row15);


    //底部布局
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

    //整体布局
    mainFlexBox.items.add(juce::FlexItem(topFlexBox).withFlex(1.0).withMargin({20, 20, 0, 20})); // 上、右、下、左 margin
    mainFlexBox.items.add(juce::FlexItem(midFlexBox).withFlex(0.8).withMargin({20, 20, 0, 20}));
    // 上、右、下、左 margin
    mainFlexBox.items.add(juce::FlexItem(bottomFlexBox).withFlex(1.0).withMargin({20, 20, 20, 20}));
    // 上、右、下、左 margin
    mainFlexBox.performLayout(area);
}

void AudioPluginAudioProcessorEditor::reloadPreset()
{
    //load preset时触发，更新一些无法设置attachment的UI，如texteditor，impulse file的加载逻辑
    if (!processorRef.isLoadingPresetFlag())
    {
        return;
    }

    //触发synth更新为最新状态：mapping所有parameter，property的值到synth中
    processorRef.getPulsarSynthEngine().executeEachPulsarSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
    {
        synth->reloadPreset(processorRef.apvts);
    });

    //加载texteditor
    String burstMask = processorRef.apvts.state.getProperty(why::PropertyID::burstMask).toString();
    String stochasticMask = processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString();
    String sampleImpulsePath = processorRef.apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString();
    //保证展示不为空，比如load preset时，也会触发回调该方法
    if (burstMaskTextEditor.getTextValue() != burstMask)
    {
        burstMaskTextEditor.setText(burstMask, juce::dontSendNotification);
    }
    if (stochasticMaskTextEditor.getTextValue() != stochasticMask)
    {
        //如果是加载preset，且property有值，而当前text无值，则不仅设置展示，还要将synth的stochasticMaskStr同步上
        stochasticMaskTextEditor.setText(stochasticMask, juce::dontSendNotification);
    }

    //加载impulse file, texteditor
    //判断当前preset中，选中的是哪一个impulse模式
    int impulseSwitch = static_cast<int>(*processorRef.apvts.getRawParameterValue(why::ParameterID::impulseSwitch));

    //当前选中的sample impulse与preset中的不同，则更新
    if (sampleImpulsePathTextEditor.getTextValue() != sampleImpulsePath)
    {
        sampleImpulsePathTextEditor.setText(sampleImpulsePath, juce::dontSendNotification);
        //没加载过impulse resouce则加载一次
        processorRef.getPulsarSynthEngine().executeEachPulsarSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            //加载资源
            juce::File file(sampleImpulsePath);
            if (file.existsAsFile())
            {
                // 文件存在，则保存资源文件到convolution resource中（全局的）
                processorRef.getPulsarSynthEngine().getConvolutionResource()->saveLastSampleFileAsBlock(file);

                //如果当前选中了sample impulse模式，则将资源文件加载为impulse
                if (impulseSwitch == static_cast<int>(why::ImpulseSwitchEnum::Sample))
                {
                    processorRef.getPulsarSynthEngine().getConvolutionResource()->loadSampleImpulseFile();
                }
            }
            else
            {
                sampleImpulsePathTextEditor.setText("File does not exist.", juce::dontSendNotification);
            }
        });
    }

    //template impulse方式
    int index = static_cast<int>(*processorRef.apvts.getRawParameterValue(why::ParameterID::impulseTemplateFile));

    //当前选中的template impulse与preset中的不同，则更新
    if (impulseTemplateFileComboBox.getSelectedItemIndex() != index)
    {
        saveThenLoadAfterSelect(juce::String(index + 1));
    }

    int currentPlayModeEnumInt = processorRef.apvts.state.getProperty(why::PropertyID::currentPlayModeEnum).toString().
                                              getIntValue();

    if (playModeCombobox.getSelectedItemIndex() != currentPlayModeEnumInt)
    {
        playModeCombobox.setSelectedItemIndex(currentPlayModeEnumInt);
    }
}

void AudioPluginAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    //reload preset
    if (source == &processorRef && processorRef.isLoadingPresetFlag())
    {
        reloadPreset();
        processorRef.setLoadingPresetFlag(false);
        return;
    }

    //非reloa preset触发（reload preset一般只发生在打开daw或打开插件窗口时）
    if (source == &processorRef)
    {
        //当train参数改变后，随机mask展示要刷新
        juce::String currentStochasticMaskStr = juce::String(processorRef.getPulsarSynthEngine().
                                                                          getCurrentPulsarSynth(processorRef.apvts)->
                                                                          getStochasticMaskStr());
        if (stochasticMaskTextEditor.getText() != currentStochasticMaskStr)
        {
            //property将会被存为state information，用来恢复参数
            processorRef.apvts.state.setProperty(why::PropertyID::stochasticMask, currentStochasticMaskStr, nullptr);
            stochasticMaskTextEditor.setText(currentStochasticMaskStr);
        }
    }
}

//===================================核心逻辑 END===========================================

void AudioPluginAudioProcessorEditor::topFlexBox(juce::FlexBox& flexBoxTop, std::shared_ptr<FlexBox> row1,
                                                 std::shared_ptr<FlexBox> row2, std::shared_ptr<FlexBox> row3,
                                                 std::shared_ptr<FlexBox> row4, std::shared_ptr<FlexBox> row5,
                                                 std::shared_ptr<FlexBox> row6,
                                                 std::shared_ptr<FlexBox> row7
)
{
    flexBoxTop.flexDirection = juce::FlexBox::Direction::column; // 水平排列（三个Slider）
    flexBoxTop.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    flexBoxTop.alignContent = juce::FlexBox::AlignContent::stretch; //在wrap换行时才有用，决定换行元素的排列
    flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    // 每一行：Label + Slider
    row1->flexDirection = juce::FlexBox::Direction::row;
    row1->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row1->items.add(juce::FlexItem(trainLenLabel).withFlex(1).withMaxWidth(100));
    // margin：上右下左
    row1->items.add(juce::FlexItem(trainLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(*row1).withFlex(1.0f));

    row2->flexDirection = juce::FlexBox::Direction::row;
    row2->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row2->items.add(juce::FlexItem(trainDutyCycleLenLabel).withFlex(1).withMaxWidth(100));
    row2->items.add(juce::FlexItem(trainDutyCycleLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(*row2).withFlex(1.0f));

    row3->flexDirection = juce::FlexBox::Direction::row;
    row3->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row3->items.add(juce::FlexItem(trainSilenceLenLabel).withFlex(1).withMaxWidth(100));
    row3->items.add(juce::FlexItem(trainSilenceLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(*row3).withFlex(1.0f));

    row4->flexDirection = juce::FlexBox::Direction::row;
    // row4.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop->justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row4->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row4->items.add(juce::FlexItem(outputGainLabel).withFlex(1).withMaxWidth(100));
    row4->items.add(juce::FlexItem(outputGainSlider).withFlex(2.5));
    row4->items.add(juce::FlexItem().withFlex(0.1));
    row4->items.add(juce::FlexItem(bpmSlider).withFlex(1));
    flexBoxTop.items.add(juce::FlexItem(*row4).withFlex(1.0f));

    row5->flexDirection = juce::FlexBox::Direction::row;
    // row4.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop->justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row5->justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    row5->items.add(juce::FlexItem(playModeLabel).withFlex(0.5).withMaxWidth(50).withMaxHeight(40));
    row5->items.add(juce::FlexItem(playModeCombobox).withFlex(0.5).withMaxWidth(70).withMaxHeight(40));

    row5->items.add(juce::FlexItem(impulseSwitchLabel).withFlex(0.5).withMaxWidth(70).withMaxHeight(40));
    row5->items.add(juce::FlexItem(impulseSwitchComboBox).withFlex(1).withMaxWidth(90).withMaxHeight(40));
    row5->items.add(juce::FlexItem(impulseTemplateLabel).withFlex(0.5).withMaxWidth(60).withMaxHeight(40));
    row5->items.add(juce::FlexItem(impulseTemplateFileComboBox).withFlex(1.0).withMaxWidth(150).withMaxHeight(40));
    // row5->items.add(juce::FlexItem().withFlex(0.2));
    row5->items.add(juce::FlexItem(selectSampleImpulseFileButton).withFlex(0.5).withMaxWidth(100).withMaxHeight(40));
    row5->items.add(juce::FlexItem(sampleImpulsePathTextEditor).withFlex(1).withMaxWidth(500).withMaxHeight(50));
    flexBoxTop.items.add(juce::FlexItem(*row5).withFlex(1.5f));
}

void AudioPluginAudioProcessorEditor::bottomFlexBox(juce::FlexBox& bottomFlexBox,
                                                    std::shared_ptr<juce::FlexBox> column1,
                                                    std::shared_ptr<juce::FlexBox> column2,
                                                    std::shared_ptr<juce::FlexBox> column3,
                                                    std::shared_ptr<juce::FlexBox> column4,
                                                    std::shared_ptr<juce::FlexBox> column5,
                                                    std::shared_ptr<juce::FlexBox> column6,
                                                    std::shared_ptr<juce::FlexBox> column7,
                                                    std::shared_ptr<juce::FlexBox> column8,
                                                    std::shared_ptr<juce::FlexBox> column9)
{
    bottomFlexBox.flexDirection = juce::FlexBox::Direction::row; // 水平排列（三个Slider）
    // flexBoxLeftBottom.flexWrap = juce::FlexBox::Wrap::noWrap; // 是否换行
    bottomFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    bottomFlexBox.alignContent = juce::FlexBox::AlignContent::flexStart; //

    column1->flexDirection = juce::FlexBox::Direction::column;
    column1->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column1->items.add(juce::FlexItem(pulsarWaveformLabel).withFlex(1.0).withMaxWidth(200).withMaxHeight(20));
    column1->items.add(juce::FlexItem(pulsarWaveformSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column1).withFlex(1.0f));

    column2->alignContent = juce::FlexBox::AlignContent::flexStart;
    column2->flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    column2->items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column2->items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column2).withFlex(1.0f));

    column3->flexDirection = juce::FlexBox::Direction::column;
    column3->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column3->items.add(
        juce::FlexItem(pulsarDutyCycleRatioLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column3->items.add(
        juce::FlexItem(pulsarDutyCycleRatioSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column3).withFlex(1.0f));

    //lfo modulation
    column4->alignContent = juce::FlexBox::AlignContent::flexStart;
    column4->flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    column4->items.add(juce::FlexItem(ampLfoLabel).withFlex(1).withMaxHeight(20));
    column4->items.add(
        juce::FlexItem(ampLfoSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column4).withFlex(1.0f));

    column5->flexDirection = juce::FlexBox::Direction::column;
    column5->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column5->items.add(
        juce::FlexItem(formantFreqLfoLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column5->items.add(
        juce::FlexItem(formantFreqLfoSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column5).withFlex(1.0f));


    //envelope
    column6->flexDirection = juce::FlexBox::Direction::column;
    column6->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column6->items.add(
        juce::FlexItem(attackLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column6->items.add(
        juce::FlexItem(attackSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column6).withFlex(1.0f));

    column7->flexDirection = juce::FlexBox::Direction::column;
    column7->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column7->items.add(
        juce::FlexItem(decayLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column7->items.add(
        juce::FlexItem(decaySlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column7).withFlex(1.0f));

    column8->flexDirection = juce::FlexBox::Direction::column;
    column8->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column8->items.add(
        juce::FlexItem(sustainLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column8->items.add(
        juce::FlexItem(sustainSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column8).withFlex(1.0f));

    column9->flexDirection = juce::FlexBox::Direction::column;
    column9->alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column9->items.add(
        juce::FlexItem(releaseLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column9->items.add(
        juce::FlexItem(releaseSlider).withFlex(2.0));
    bottomFlexBox.items.add(juce::FlexItem(*column9).withFlex(1.0f));

    // 调整控件大小
    // flexBoxLeftBottom.performLayout(area);
}

void AudioPluginAudioProcessorEditor::midFlexBox(juce::FlexBox& midFlexBox, std::shared_ptr<juce::FlexBox> row11,
                                                 std::shared_ptr<juce::FlexBox> row12,
                                                 std::shared_ptr<juce::FlexBox> row13,
                                                 std::shared_ptr<juce::FlexBox> row14,
                                                 std::shared_ptr<juce::FlexBox> row15)
{
    midFlexBox.flexDirection = juce::FlexBox::Direction::row; // 水平排列（三个Slider）

    row11->flexDirection = juce::FlexBox::Direction::row;
    row11->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row11->items.add(juce::FlexItem(maskComboBoxLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row11->items.add(juce::FlexItem(maskOptionComboBox).withFlex(1.0).withMaxHeight(20));
    midFlexBox.items.add(juce::FlexItem(*row11).withFlex(1.0f));

    row12->flexDirection = juce::FlexBox::Direction::row;
    row12->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row12->items.add(juce::FlexItem(burstMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row12->items.add(
        juce::FlexItem(burstMaskTextEditor).withFlex(1.0).withMinWidth(100).withMaxWidth(250).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*row12).withFlex(1.5f));

    row13->flexDirection = juce::FlexBox::Direction::row;
    // flexBoxTop->justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row13->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row13->items.add(juce::FlexItem(euclidStepLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row13->items.add(
        juce::FlexItem(euclidStepDial).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*row13).withFlex(0.9f));

    row14->flexDirection = juce::FlexBox::Direction::row;
    row14->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row14->items.add(juce::FlexItem(euclidHitLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row14->items.add(juce::FlexItem(euclidHitDial).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*row14).withFlex(0.9f));

    row15->flexDirection = juce::FlexBox::Direction::row;
    row15->justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row15->items.add(juce::FlexItem(stochasticMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row15->items.add(juce::FlexItem(stochasticMaskTextEditor).withFlex(1.0).withMaxWidth(250).withMaxHeight(150));
    midFlexBox.items.add(juce::FlexItem(*row15).withFlex(1.5f));

    // flexBoxMiddle.performLayout(area.removeFromTop(150));
    //
    // area.removeFromTop(20); // 插入 20px 空白
}

// void AudioPluginAudioProcessorEditor::bindParameterListener()
// {
//     //为所有参数增加监听（监听具体的参数变化）
//     for (int i = 0; i < processorRef.apvts.state.getNumChildren(); ++i)
//     {
//         auto child = processorRef.apvts.state.getChild(i);
//         if (child.hasType("PARAM") && child.hasProperty("id"))
//         {
//             juce::String paramID = child["id"];
//             processorRef.apvts.addParameterListener(paramID, this);
//         }
//     }
// }

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
    addAndMakeVisible(euclidStepDial);
    addAndMakeVisible(euclidStepLabel);
    addAndMakeVisible(euclidHitDial);
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

void AudioPluginAudioProcessorEditor::setUIStyle()
{
    //output
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    outputGainSlider.setTextValueSuffix(" (db)");

    outputGainLabel.setText("Output", juce::dontSendNotification);

    //play mode
    playModeCombobox.addItem("Auto", static_cast<int>(why::PlayModeEnum::Auto) + 1);
    //item id不能为0，但是parameter获取到的值是从0开始
    playModeCombobox.addItem("Midi", static_cast<int>(why::PlayModeEnum::Midi) + 1);
    playModeCombobox.setSelectedId(1); // 默认选中第一个选项

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
    pulsarWaveformSlider.setRange(0, 1, 0);

    pulsarWaveformLabel.setText("Pulsar Waveform", juce::dontSendNotification);
    // pulsarWaveformLabel.attachToComponent(&pulsarWaveformSlider, true);

    pulsarDutyCycleClusterLenSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulsarDutyCycleClusterLenSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    pulsarDutyCycleClusterLenSlider.setTextValueSuffix("");

    pulsarDutyCycleClusterLenLabel.setText("Pulsar Duty Cycle Cluster", juce::dontSendNotification);
    // pulsarDutyCycleClusterLenLabel.attachToComponent(&pulsarDutyCycleClusterLenSlider, true);


    pulsarDutyCycleRatioSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulsarDutyCycleRatioSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    pulsarDutyCycleRatioSlider.setTextValueSuffix("");

    pulsarDutyCycleRatioLabel.setText("Pulsar Duty Cycle Ratio", juce::dontSendNotification);
    // pulsarDutyCycleRatioLabel.attachToComponent(&pulsarDutyCycleRatioSlider, true);

    //lfo
    ampLfoSlider.setSliderStyle(juce::Slider::LinearVertical);
    ampLfoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    ampLfoSlider.setTextValueSuffix("");

    ampLfoLabel.setText("AM Waveform", juce::dontSendNotification);


    formantFreqLfoSlider.setSliderStyle(juce::Slider::LinearVertical);
    formantFreqLfoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    formantFreqLfoSlider.setTextValueSuffix("");

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
    maskOptionComboBox.setSelectedId(1); // 默认选中第一个选项

    impulseTemplateLabel.setText("Template", juce::dontSendNotification);
    // impulseTemplateFileComboBox.setSelectedId(1); // 默认选中第一个选项

    burstMaskLabel.setText("Burst Mask", juce::dontSendNotification);

    burstMaskTextEditor.setMultiLine(true);
    burstMaskTextEditor.setScrollbarsShown(true);
    burstMaskTextEditor.setPopupMenuEnabled(false);
    burstMaskTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
    burstMaskTextEditor.setInputRestrictions(256, "01"); //限制只输入0或1
    burstMaskTextEditor.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(
                                           256, // 最长 128 个字符（你可以改）
                                           "01" // 只允许字符 '0' 和 '1'
                                       ), true); // true 表示替换当前 filter
    burstMaskTextEditor.setTextToShowWhenEmpty("Only 0 or 1 can be input", juce::Colours::grey);

    euclidStepLabel.setText("Euclid Step", juce::dontSendNotification);

    euclidStepDial.setSliderStyle(juce::Slider::LinearVertical);
    euclidStepDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    euclidStepDial.setRange(1, 32, 1);
    euclidStepDial.setNumDecimalPlacesToDisplay(0); // 不显示小数

    euclidHitLabel.setText("Euclid Hit", juce::dontSendNotification);

    euclidHitDial.setSliderStyle(juce::Slider::LinearVertical);
    euclidHitDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    euclidHitDial.setRange(1, 32, 1);
    euclidHitDial.setNumDecimalPlacesToDisplay(0); // 不显示小数

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
    impulseSwitchComboBox.setSelectedId(1);

    impulseSwitchLabel.setText("Convolution", juce::dontSendNotification);

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

void AudioPluginAudioProcessorEditor::connectUIAndAudioParameter()
{
    //绑定UI与Parameter
    // 将你的 UI 控件（Slider）绑定到一个音频参数（parameter）上，确保 UI 与内部参数同步，从而能够在DAW中automation，preset相关
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::outputGain, outputGainSlider);

    playModeComboxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
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
        processorRef.apvts, why::ParameterID::euclidSteps, euclidStepDial);
    euclidHitDialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, why::ParameterID::euclidHits, euclidHitDial);

    //impulse file
    impulseSwitchComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::impulseSwitch, impulseSwitchComboBox);
    impulseTemplateFileComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, why::ParameterID::impulseTemplateFile, impulseTemplateFileComboBox);
}

void AudioPluginAudioProcessorEditor::initUITriggerEvent()
{
    //选择sample impulse的按钮，点击事件
    selectSampleImpulseFileButton.onClick = [this]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        openFileChooser();
    };

    //burst mask text editor回车，没有attachment，需要手动更新synth状态
    burstMaskTextEditor.onReturnKey = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        // 监听 TextEditor 内容变化
        auto currentBurstMaskText = burstMaskTextEditor.getText(); // 获取编辑框中的文本

        //刷新synth的burst mask标记
        processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->refreshBurstMask(currentBurstMaskText);
        });

        //保存到property，在load preset时，可从parameterChanged监听中获得property值，进行手动监听
        processorRef.apvts.state.setProperty(why::PropertyID::burstMask, currentBurstMaskText, nullptr);

        //效果变化
        auto originalColour = burstMaskTextEditor.findColour(juce::TextEditor::backgroundColourId);

        // 临时高亮
        burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::mediumaquamarine);
        burstMaskTextEditor.repaint(); // 强制重绘

        //还原
        juce::Timer::callAfterDelay(500, [&, originalColour]()
        {
            burstMaskTextEditor.setColour(juce::TextEditor::backgroundColourId, originalColour);
            burstMaskTextEditor.repaint();
        });
    };

    //euclid联动设置
    euclidStepDial.onValueChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        rebalanceStepHitValueDisplay();
    };

    euclidHitDial.onValueChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        rebalanceStepHitValueDisplay();
    };

    //选择template impulse下拉框
    impulseTemplateFileComboBox.onChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        saveThenLoadAfterSelect(juce::String(impulseTemplateFileComboBox.getSelectedId()));
    };

    //reload preset时，如果数据变化也会体现在event
    impulseSwitchComboBox.onChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        //选择了template impulse或sample impulse选项，直接加载impulse file
        if (impulseSwitchComboBox.getSelectedId() == static_cast<int>(why::ImpulseSwitchEnum::Template) + 1)
        {
            //如果资源没加载（比如默认选中的第一个文件，是没做初始化的），要先加载，避免reload preset进来时，资源还没有加载
            saveThenLoadAfterSelect(juce::String(impulseTemplateFileComboBox.getSelectedId()));
        }
        if (impulseSwitchComboBox.getSelectedId() == static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1)
        {
            //有两处地方可以加载资源：1，手动选择文件，2，load preset时，广播监听触发，
            //因此，这里只判断加载impulse即可，不会出现点击combobox时，sample资源还没加载的情况
            loadSampleImpulseWhenSelected();
        }
    };

    playModeCombobox.onChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        //TODO 切换模式后，会走到prameterChanged重新设置train，直接重置train

        if (playModeCombobox.getSelectedId() == static_cast<int>(why::PlayModeEnum::Auto) + 1)
        {
            processorRef.getPulsarSynthEngine().setCurrentPlayModeEnum(processorRef.apvts, why::PlayModeEnum::Auto);
        }
        if (playModeCombobox.getSelectedId() == static_cast<int>(why::PlayModeEnum::Midi) + 1)
        {
            processorRef.getPulsarSynthEngine().setCurrentPlayModeEnum(processorRef.apvts, why::PlayModeEnum::Midi);
        }
    };

    //强制更新synth依赖的bpm
    bpmSlider.onValueChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        processorRef.getPulsarSynthEngine().executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->forceRefreshBpm(bpmSlider.getValue());
        });
    };

    maskOptionComboBox.onChange = [&]
    {
        if (processorRef.isLoadingPresetFlag())
        {
            return;
        }
        //默认采用第一个展示，所有synth都一样
        std::string maskStrStd = processorRef.getPulsarSynthEngine().getCurrentPulsarSynth(processorRef.apvts)->
                                              getStochasticMaskStr();
        juce::String stochasticMaskStr = juce::String(maskStrStd);
        //只在选中stochastic mask时才展示生成的随机mask
        if (maskOptionComboBox.getSelectedItemIndex() == static_cast<int>(why::MaskOptionEnum::StochasticMask))
        {
            stochasticMaskTextEditor.setText(stochasticMaskStr, juce::dontSendNotification);
        }
    };
}

void AudioPluginAudioProcessorEditor::rebalanceStepHitValueDisplay()
{
    double stepValue = euclidStepDial.getValue();
    double hitValue = euclidHitDial.getValue();

    // 更新 sliderB 的最大值
    if (stepValue <= 1)
    {
        //juce的逻辑：range不可以设置为1，1，必须满足max>min
        //所以step为1，hit不设为1了，直接禁用
        euclidHitDial.setValue(1);
        euclidHitDial.setEnabled(false);
    }
    else
    {
        euclidHitDial.setEnabled(true);
        euclidHitDial.setRange(euclidHitDial.getMinimum(), stepValue);
    }

    if (hitValue > stepValue)
        euclidHitDial.setValue(stepValue, juce::dontSendNotification); // 避免无限触发
}

void AudioPluginAudioProcessorEditor::saveThenLoadAfterSelect(juce::String selectedId)
{
    const char* resourceName = (BinaryResourceSingleton::getInstance().getBinaryIdFileNameMap()[selectedId]).
        toRawUTF8();
    double fileSampleRate;
    std::unique_ptr<juce::AudioBuffer<float>> bf;

    if (why::readFileFromResources(resourceName, fileSampleRate, bf))
    {
        processorRef.getPulsarSynthEngine().getConvolutionResource()->saveLastTemplateImpulseData(
            *bf, fileSampleRate, std::string(resourceName));
        loadTemplateImpulseWhenSelected();
    }
}

void AudioPluginAudioProcessorEditor::setWindowSize()
{
    // Make sure that before the constructor has finished, you've set the editor's size to whatever you need it to be.
    // setSize(1200, 600);
    // 允许窗口被用户调整大小
    setResizable(true, true);
    // 设置最小和最大尺寸
    setResizeLimits(1200, 600, 2560, 1440);
}


void AudioPluginAudioProcessorEditor::openFileChooser()
{
    // 创建一个文件选择器
    fileChooser = std::make_unique<juce::FileChooser>("Select a file to upload",
                                                      juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                      "*.wav;*.mp3");

    //读取窗口所选文件
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

void AudioPluginAudioProcessorEditor::loadSampleImpulseWhenSelected()
{
    if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1)
    {
        return;
    }
    processorRef.getPulsarSynthEngine().getConvolutionResource()->loadSampleImpulseFile();
}

void AudioPluginAudioProcessorEditor::saveFileIntoSynth(const juce::File& file)
{
    processorRef.getPulsarSynthEngine().getConvolutionResource()->saveLastSampleFileAsBlock(file);
}

void AudioPluginAudioProcessorEditor::loadTemplateImpulseWhenSelected()
{
    //当前如果是template模式则直接加载
    if (impulseSwitchComboBox.getSelectedId() != static_cast<int>(why::ImpulseSwitchEnum::Template) + 1)
    {
        return;
    }
    processorRef.getPulsarSynthEngine().getConvolutionResource()->loadTemplateImpulseFile();
}

void AudioPluginAudioProcessorEditor::initTemplateImpulseComboboxNames()
{
    // impulseTemplateFileComboBox.removeAllChildren();

    // 塞入 BinaryData 中所有资源文件名
    for (int i = 0; i < BinaryResourceSingleton::getInstance().getBinaryIdFileNameMap().size(); ++i)
    {
        impulseTemplateFileComboBox.addItem(
            BinaryResourceSingleton::getInstance().getBinaryIdFileNameMap().at(juce::String(i + 1)),
            i + 1); // ID 从 1 开始,apvts返回的parametervalue是从0开始
    }

    // 可选：设置默认选择项
    impulseTemplateFileComboBox.setSelectedId(1, juce::dontSendNotification);
}

//TODO  废弃，这种最好写到pluginprocessor，改用广播了目前
void AudioPluginAudioProcessorEditor::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged,
                                                               const Identifier& property)
{
    // apvts.state.addListener(*this);
    // std::string newValue = processorRef.apvts.state.getProperty(why::PropertyID::stochasticMask).toString().
    //                                     toStdString();
    // if (property.toString() == why::PropertyID::stochasticMask && newValue != stochasticMaskTextEditor.getText())
    // {
    //     stochasticMaskTextEditor.setText(newValue, juce::dontSendNotification);
    // }
}
