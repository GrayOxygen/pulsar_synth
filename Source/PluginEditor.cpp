#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused(processorRef);

    //为了所有参数增加监听（监听具体的参数变化）
    for (int i = 0; i < processorRef.apvts.state.getNumChildren(); ++i)
    {
        auto child = processorRef.apvts.state.getChild(i);
        if (child.hasType("PARAM") && child.hasProperty("id"))
        {
            juce::String paramID = child["id"];
            processorRef.apvts.addParameterListener(paramID, this);
        }
    }

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    // setSize(400, 300);
    // 允许窗口被用户调整大小
    setResizable(true, true);
    // 设置最小和最大尺寸
    setResizeLimits(1200, 600, 2560, 1440);

    addAndMakeVisible(trainLenSlider);
    addAndMakeVisible(trainDutyCycleLenSlider);
    addAndMakeVisible(trainSilenceLenSlider);

    addAndMakeVisible(trainLenLabel);
    addAndMakeVisible(trainDutyCycleLenLabel);
    addAndMakeVisible(trainSilenceLenLabel);

    addAndMakeVisible(outputGainLabel);
    addAndMakeVisible(outputGainSlider);

    addAndMakeVisible(pulsarWaveformSlider);
    addAndMakeVisible(pulsarDutyCycleRatioSlider);
    addAndMakeVisible(pulsarDutyCycleClusterLenSlider);

    addAndMakeVisible(pulsarWaveformLabel);
    addAndMakeVisible(pulsarDutyCycleRatioLabel);
    addAndMakeVisible(pulsarDutyCycleClusterLenLabel);


    addAndMakeVisible(ampLfoSlider);
    addAndMakeVisible(formantFreqLfoSlider);

    addAndMakeVisible(ampLfoLabel);
    addAndMakeVisible(formantFreqLfoLabel);

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

    // convolution impulse
    addAndMakeVisible(selectImpulseButton);
    addAndMakeVisible(impulseTemplateLabel);
    addAndMakeVisible(impulseTemplateComboBox);
    addAndMakeVisible(impulseSwitchComboBox);
    addAndMakeVisible(impulsePathTextEditor);
    addAndMakeVisible(impulseSwitchLabel);

    //output
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    outputGainSlider.setTextValueSuffix(" (db)");

    outputGainLabel.setText("Output", juce::dontSendNotification);

    //train
    trainLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trainLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    trainLenSlider.setTextValueSuffix(" (1/64)");

    trainLenLabel.setText("Train Length", juce::dontSendNotification);
    // trainLenLabel.attachToComponent(&trainLenSlider, true);


    trainDutyCycleLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trainDutyCycleLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    trainDutyCycleLenSlider.setTextValueSuffix(" (count)");

    trainDutyCycleLenLabel.setText("Train Pulsar", juce::dontSendNotification);
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

    ampLfoLabel.setText("Amp Mod", juce::dontSendNotification);


    formantFreqLfoSlider.setSliderStyle(juce::Slider::LinearVertical);
    formantFreqLfoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    formantFreqLfoSlider.setTextValueSuffix("");

    formantFreqLfoLabel.setText("Freq Mod", juce::dontSendNotification);


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

    impulseTemplateLabel.setText("Samples", juce::dontSendNotification);
    impulseTemplateComboBox.setSelectedId(1); // 默认选中第一个选项

    burstMaskLabel.setText("BurstMask", juce::dontSendNotification);

    burstMaskTextEditor.setMultiLine(true);
    burstMaskTextEditor.setScrollbarsShown(true);
    burstMaskTextEditor.setPopupMenuEnabled(false);
    burstMaskTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
    burstMaskTextEditor.setInputRestrictions(0, "01"); //限制只输入0或1
    burstMaskTextEditor.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(
                                           128 * 10, // 最长 128 个字符（你可以改）
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
    selectImpulseButton.setButtonText("Select Impulse");
    selectImpulseButton.onClick = [this]
    {
        openFileChooser();
    };

    impulseSwitchComboBox.addItem("Off", static_cast<int>(why::ImpulseSwitchEnum::Off) + 1);
    impulseSwitchComboBox.addItem("Template", static_cast<int>(why::ImpulseSwitchEnum::Template) + 1);
    impulseSwitchComboBox.addItem("Sample", static_cast<int>(why::ImpulseSwitchEnum::Sample) + 1);
    impulseSwitchComboBox.setSelectedId(1);

    impulseSwitchLabel.setText("Convolution", juce::dontSendNotification);

    impulsePathTextEditor.setMultiLine(true);
    impulsePathTextEditor.setReadOnly(true);
    impulsePathTextEditor.setScrollbarsShown(true);
    impulsePathTextEditor.setPopupMenuEnabled(false);
    impulsePathTextEditor.setReturnKeyStartsNewLine(false); // 按回车不换行（默认也是 false）
    impulsePathTextEditor.setInputRestrictions(0, "01"); //限制只输入0或1
    impulsePathTextEditor.setTextToShowWhenEmpty("No selected file", juce::Colours::grey);

    //绑定UI与Parameter
    // 将你的 UI 控件（Slider）绑定到一个音频参数（parameter）上，确保 UI 与内部参数同步，从而能够在DAW中automation，preset相关
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "outputGain", outputGainSlider);

    trainLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "trainLen", trainLenSlider);
    trainDutyCycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "trainDutyCycleLen", trainDutyCycleLenSlider);
    trainSilenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "trainSilenceLen", trainSilenceLenSlider);

    pulsarWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarWaveform", pulsarWaveformSlider);
    pulsarDutyCycleClusterLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarDutyCycleClusterLen", pulsarDutyCycleClusterLenSlider);
    pulsarDutyCycleRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarDutyCycleRatio", pulsarDutyCycleRatioSlider);

    ampLfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "ampLfoWaveform", ampLfoSlider);
    formantFreqLfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "formantFreqLfoWaveform", formantFreqLfoSlider);

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarAttack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarDecay", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarSustain", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pulsarRelease", releaseSlider);

    maskOptionComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "maskOption", maskOptionComboBox);
    euclidStepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "euclidSteps", euclidStepDial);
    euclidHitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "euclidHits", euclidHitDial);

    //impulse file
    impulseSwitchComboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "impulseSwitch", impulseSwitchComboBox);


    //触发事件
    //text editor没有combobox
    burstMaskTextEditor.onReturnKey = [&]
    {
        // 监听 TextEditor 内容变化
        auto currentText = burstMaskTextEditor.getText(); // 获取编辑框中的文本
        //只有property才能塞下string，property变化则要通过tree value listener监听到变化，AudioProcessorValueTreeState::Listener只是单个parameter变化监听
        // processorRef.apvts.state.setProperty("burstMask", currentText, nullptr); // 存储在 ValueTree 中
        //或者直接修改pulsar
        processorRef.get_pulsar()->refresh_burst_mask(currentText);
    };

    euclidStepDial.onValueChange = [&]
    {
        double aValue = euclidStepDial.getValue();
        double bValue = euclidHitDial.getValue();

        // 更新 sliderB 的最大值
        if (aValue <= 0)
        {
            euclidHitDial.setRange(0, 0);
        }
        else
        {
            euclidHitDial.setRange(euclidHitDial.getMinimum(), aValue);
        }

        if (bValue > aValue)
            euclidHitDial.setValue(aValue, juce::dontSendNotification); // 避免无限触发
    };

    impulseTemplateComboBox.onChange = [&]
    {
        //读取template文件
        int dataSize = 0;
        const void* data = BinaryData::getNamedResource(
            (binaryIdFileNameMap[static_cast<juce::String>(impulseTemplateComboBox.getSelectedId())]).toRawUTF8(),
            dataSize);

        // data 是二进制数据的起始地址，dataSize 是它的大小
        if (data != nullptr)
        {
            std::unique_ptr<juce::MemoryInputStream> stream;
            stream.reset(new juce::MemoryInputStream(data, static_cast<size_t>(dataSize), false));
            // 例如加载成 AudioBuffer
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats(); // 支持 WAV, AIFF 等常见格式

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(stream)));

            if (reader != nullptr)
            {
                juce::AudioBuffer<float> buffer(reader->numChannels, static_cast<int>(reader->lengthInSamples));
                reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
                // 现在 buffer 就是你加载好的 impulse data

                //更新impulse file
                std::unique_ptr<juce::AudioBuffer<float>> bf = std::make_unique<juce::AudioBuffer<float>>(buffer);
                processorRef.set_last_template_buffer(bf);
                processorRef.set_last_template_data_size(reader->sampleRate);


                loadTemplateImpulse();
            }
        }
    };

    //初始化下拉框列表
    buildImpulseComboboxNames();
}

//==============================================================================

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // stopTimer(); // 停止定时器
    //移除绑定
    for (int i = 0; i < processorRef.apvts.state.getNumChildren(); ++i)
    {
        auto child = processorRef.apvts.state.getChild(i);
        if (child.hasType("PARAM") && child.hasProperty("id"))
        {
            juce::String paramID = child["id"];
            processorRef.apvts.removeParameterListener(paramID, this);
        }
    }
}

//==============================================================================
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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    //设置UI大小
    //    visualiser.setBounds(getLocalBounds());

    // 获取当前窗口的区域
    auto area = getLocalBounds();

    juce::FlexBox mainFlexBox;
    mainFlexBox.flexDirection = juce::FlexBox::Direction::column;

    //====================================头部====================================
    juce::FlexBox flexBoxTop;

    flexBoxTop.flexDirection = juce::FlexBox::Direction::column; // 水平排列（三个Slider）
    flexBoxTop.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    flexBoxTop.alignContent = juce::FlexBox::AlignContent::stretch; //在wrap换行时才有用，决定换行元素的排列
    flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    // 每一行：Label + Slider
    juce::FlexBox row1;
    row1.flexDirection = juce::FlexBox::Direction::row;
    row1.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row1.items.add(juce::FlexItem(trainLenLabel).withFlex(1).withMaxWidth(80));
    // margin：上右下左
    row1.items.add(juce::FlexItem(trainLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(row1).withFlex(1.0f));

    juce::FlexBox row2;
    row2.flexDirection = juce::FlexBox::Direction::row;
    row2.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row2.items.add(juce::FlexItem(trainDutyCycleLenLabel).withFlex(1).withMaxWidth(80));
    row2.items.add(juce::FlexItem(trainDutyCycleLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(row2).withFlex(1.0f));

    juce::FlexBox row3;
    row3.flexDirection = juce::FlexBox::Direction::row;
    row3.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row3.items.add(juce::FlexItem(trainSilenceLenLabel).withFlex(1).withMaxWidth(80));
    row3.items.add(juce::FlexItem(trainSilenceLenSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(row3).withFlex(1.0f));

    juce::FlexBox row4;
    row4.flexDirection = juce::FlexBox::Direction::row;
    // row4.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row4.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row4.items.add(juce::FlexItem(outputGainLabel).withFlex(1).withMaxWidth(80));
    row4.items.add(juce::FlexItem(outputGainSlider).withFlex(2.5));
    flexBoxTop.items.add(juce::FlexItem(row4).withFlex(1.0f));

    juce::FlexBox row5;
    row5.flexDirection = juce::FlexBox::Direction::row;
    // row4.flexWrap = juce::FlexBox::Wrap::wrap; // 是否换行
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row5.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row5.items.add(juce::FlexItem(impulseSwitchLabel).withFlex(0.5).withMaxWidth(50).withMaxHeight(50));
    row5.items.add(juce::FlexItem(impulseSwitchComboBox).withFlex(0.5).withMaxWidth(200).withMaxHeight(50));
    row5.items.add(juce::FlexItem(impulseTemplateLabel).withFlex(0.5).withMaxWidth(50));
    row5.items.add(juce::FlexItem(impulseTemplateComboBox).withFlex(0.5).withMaxWidth(150));
    row5.items.add(juce::FlexItem().withFlex(0.2));
    row5.items.add(juce::FlexItem(selectImpulseButton).withFlex(1).withMaxWidth(200).withMaxHeight(50));
    row5.items.add(juce::FlexItem(impulsePathTextEditor).withFlex(1).withMaxWidth(500).withMaxHeight(50));
    flexBoxTop.items.add(juce::FlexItem(row5).withFlex(0.7f));

    // 调整控件大小
    // flexBoxTop.performLayout(area.removeFromTop(200));
    // area.removeFromTop(20); // 插入 20px 空白

    //====================================中部====================================
    //masking
    juce::FlexBox flexBoxMiddle;
    flexBoxMiddle.flexDirection = juce::FlexBox::Direction::row; // 水平排列（三个Slider）
    // flexBoxMiddle.flexWrap = juce::FlexBox::Wrap::noWrap; // 是否换行
    // flexBoxMiddle.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    // flexBoxMiddle.alignContent = juce::FlexBox::AlignContent::flexStart;

    juce::FlexBox row11;
    row11.flexDirection = juce::FlexBox::Direction::row;
    row11.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row11.items.add(juce::FlexItem(maskComboBoxLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row11.items.add(juce::FlexItem(maskOptionComboBox).withFlex(1.0).withMaxHeight(20));
    flexBoxMiddle.items.add(juce::FlexItem(row11).withFlex(1.0f));

    juce::FlexBox row12;
    row12.flexDirection = juce::FlexBox::Direction::row;
    row12.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row12.items.add(juce::FlexItem(burstMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row12.items.add(
        juce::FlexItem(burstMaskTextEditor).withFlex(1.0).withMinWidth(100).withMaxWidth(250).withMaxHeight(150));
    flexBoxMiddle.items.add(juce::FlexItem(row12).withFlex(1.5f));

    juce::FlexBox row13;
    row13.flexDirection = juce::FlexBox::Direction::row;
    // flexBoxTop.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    row13.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row13.items.add(juce::FlexItem(euclidStepLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row13.items.add(
        juce::FlexItem(euclidStepDial).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    flexBoxMiddle.items.add(juce::FlexItem(row13).withFlex(0.9f));

    juce::FlexBox row14;
    row14.flexDirection = juce::FlexBox::Direction::row;
    row14.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row14.items.add(juce::FlexItem(euclidHitLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row14.items.add(juce::FlexItem(euclidHitDial).withFlex(1.0).withMaxWidth(50).withMaxHeight(150));
    flexBoxMiddle.items.add(juce::FlexItem(row14).withFlex(0.9f));

    juce::FlexBox row15;
    row15.flexDirection = juce::FlexBox::Direction::row;
    row15.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    row15.items.add(juce::FlexItem(stochasticMaskLabel).withFlex(1.0).withMaxWidth(100).withMaxHeight(20));
    row15.items.add(juce::FlexItem(stochasticMaskTextEditor).withFlex(1.0).withMaxWidth(250).withMaxHeight(150));
    flexBoxMiddle.items.add(juce::FlexItem(row15).withFlex(1.5f));

    // flexBoxMiddle.performLayout(area.removeFromTop(150));
    //
    // area.removeFromTop(20); // 插入 20px 空白

    //====================================底部====================================
    juce::FlexBox flexBoxLeftBottom;

    flexBoxLeftBottom.flexDirection = juce::FlexBox::Direction::row; // 水平排列（三个Slider）
    // flexBoxLeftBottom.flexWrap = juce::FlexBox::Wrap::noWrap; // 是否换行
    flexBoxLeftBottom.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // 控件均匀分布
    flexBoxLeftBottom.alignContent = juce::FlexBox::AlignContent::flexStart; //

    juce::FlexBox column1;
    column1.flexDirection = juce::FlexBox::Direction::column;
    column1.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column1.items.add(juce::FlexItem(pulsarWaveformLabel).withFlex(1.0).withMaxWidth(200).withMaxHeight(20));
    column1.items.add(juce::FlexItem(pulsarWaveformSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column1).withFlex(1.0f));

    juce::FlexBox column2;
    column2.alignContent = juce::FlexBox::AlignContent::flexStart;
    column2.flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    column2.items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column2.items.add(
        juce::FlexItem(pulsarDutyCycleClusterLenSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column2).withFlex(1.0f));

    juce::FlexBox column3;
    column3.flexDirection = juce::FlexBox::Direction::column;
    column3.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column3.items.add(
        juce::FlexItem(pulsarDutyCycleRatioLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column3.items.add(
        juce::FlexItem(pulsarDutyCycleRatioSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column3).withFlex(1.0f));

    //lfo modulation
    juce::FlexBox column4;
    column4.alignContent = juce::FlexBox::AlignContent::flexStart;
    column4.flexDirection = juce::FlexBox::Direction::column;
    // margin：上右下左
    column4.items.add(juce::FlexItem(ampLfoLabel).withFlex(1).withMaxHeight(20));
    column4.items.add(
        juce::FlexItem(ampLfoSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column4).withFlex(1.0f));

    juce::FlexBox column5;
    column5.flexDirection = juce::FlexBox::Direction::column;
    column5.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column5.items.add(
        juce::FlexItem(formantFreqLfoLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column5.items.add(
        juce::FlexItem(formantFreqLfoSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column5).withFlex(1.0f));


    //envelope
    juce::FlexBox column6;
    column6.flexDirection = juce::FlexBox::Direction::column;
    column6.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column6.items.add(
        juce::FlexItem(attackLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column6.items.add(
        juce::FlexItem(attackSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column6).withFlex(1.0f));


    juce::FlexBox column7;
    column7.flexDirection = juce::FlexBox::Direction::column;
    column7.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column7.items.add(
        juce::FlexItem(decayLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column7.items.add(
        juce::FlexItem(decaySlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column7).withFlex(1.0f));


    juce::FlexBox column8;
    column8.flexDirection = juce::FlexBox::Direction::column;
    column8.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column8.items.add(
        juce::FlexItem(sustainLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column8.items.add(
        juce::FlexItem(sustainSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column8).withFlex(1.0f));


    juce::FlexBox column9;
    column9.flexDirection = juce::FlexBox::Direction::column;
    column9.alignContent = juce::FlexBox::AlignContent::flexStart;
    // margin：上右下左
    column9.items.add(
        juce::FlexItem(releaseLabel).withFlex(1).withMaxWidth(200).withMaxHeight(20));
    column9.items.add(
        juce::FlexItem(releaseSlider).withFlex(2.0));
    flexBoxLeftBottom.items.add(juce::FlexItem(column9).withFlex(1.0f));

    // 调整控件大小
    // flexBoxLeftBottom.performLayout(area);

    mainFlexBox.items.add(juce::FlexItem(flexBoxTop).withFlex(1.0).withMargin({20, 20, 20, 20})); // 上、右、下、左 margin
    mainFlexBox.items.add(juce::FlexItem(flexBoxMiddle).withFlex(0.8).withMargin({20, 20, 0, 20}));
    // 上、右、下、左 margin
    mainFlexBox.items.add(juce::FlexItem(flexBoxLeftBottom).withFlex(1.0).withMargin({20, 20, 20, 20}));
    // 上、右、下、左 margin
    mainFlexBox.performLayout(area);
}

void AudioPluginAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    //synth变更
    processorRef.get_pulsar()->parameterChanged(processorRef.apvts, parameterID);
    if (parameterID == "impulseSwitch")
    {
        //切换impulse源，重新加载impulse
        int impulseSwitch = static_cast<int>(processorRef.apvts.getRawParameterValue("impulseSwitch")->load());
        if (impulseSwitch == static_cast<int>(why::ImpulseSwitchEnum::Template))
        {
            loadTemplateImpulse();
        }
        if (impulseSwitch == static_cast<int>(why::ImpulseSwitchEnum::Sample))
        {
            loadSampleImpulse();
        }
    }
    //ui变更
    //只在选中stochastic mask时才展示生成的随机mask
    if (static_cast<int>(processorRef.apvts.getRawParameterValue("maskOption")->load()) == static_cast<int>(
        why::MaskOptionEnum::StochasticMask))
    {
        // maskOption = why::MaskOption::StochasticMask;
        stochasticMaskTextEditor.setText(processorRef.get_pulsar()->get_stochastic_mask_str(),
                                         juce::dontSendNotification);
    }
}

void AudioPluginAudioProcessorEditor::buildImpulseComboboxNames()
{
    // 塞入 BinaryData 中所有资源文件名
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String filename = BinaryData::namedResourceList[i];
        impulseTemplateComboBox.addItem(filename, i + 1); // ID 从 1 开始,apvts返回的parametervalue是从0开始
        binaryIdFileNameMap[static_cast<juce::String>(i + 1)] = filename;
    }

    // 可选：设置默认选择项
    impulseTemplateComboBox.setSelectedId(1);
}

//===废弃===
// 更新波形的函数
void AudioPluginAudioProcessorEditor::updateWaveform(const juce::AudioBuffer<float>& buffer)
{
    // 向可视化组件传递缓冲区数据
    //    visualiser.pushBuffer(buffer);
}

/**
 * 自动回调
 */
void AudioPluginAudioProcessorEditor::timerCallback()
{
    //    // 每次定时器触发时更新波形
    //    juce::AudioBuffer<float> &buffer = processorRef.getAudioBuffer();
    //    if (&buffer != nullptr) {
    //    // 只更新当前音频缓冲区的部分数据
    //        visualiser.pushBuffer(buffer);
    //    }
}
