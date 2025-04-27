#include "PluginProcessor.h"

#include <ResourceSingleton.h>

#include "PluginEditor.h"
#include "PulsarSynth.h"
//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                                 .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ), apvts(*this, nullptr, "ParamTree", createParameterLayout())
{
    // //监听valuetree listener
    // apvts.state.addListener(*this);

    //为所有参数增加apvts监听（监听具体的参数变化）
    for (int i = 0; i < apvts.state.getNumChildren(); ++i)
    {
        auto child = apvts.state.getChild(i);
        if (child.hasType("PARAM") && child.hasProperty("id"))
        {
            juce::String paramID = child["id"];
            apvts.addParameterListener(paramID, this);
        }
    }

    pulsarSynthEngine.init(apvts);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    for (int i = 0; i < apvts.state.getNumChildren(); ++i)
    {
        auto child = apvts.state.getChild(i);
        if (child.hasType("PARAM") && child.hasProperty("id"))
        {
            juce::String paramID = child["id"];
            apvts.removeParameterListener(paramID, this);
        }
    }
}


//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);

    pulsarSynthEngine.buildTrain(sampleRate, samplesPerBlock, getNumOutputChannels(), getPlayHead());

    //手动触发parameterChanged监听：因为插件窗口未打开时，automation只会自动修改apvts的参数，不触发parameterChagned监听
    for (auto& param : getParameters())
    {
        if (auto* ap = dynamic_cast<juce::AudioParameterFloat*>(param))
        {
            paramMap[ap->paramID] = apvts.getRawParameterValue(ap->paramID);
            oldParamMap[ap->paramID] = paramMap[ap->paramID]->load();
        }
        if (auto* ap = dynamic_cast<juce::AudioParameterInt*>(param))
        {
            paramMap[ap->paramID] = apvts.getRawParameterValue(ap->paramID);
            oldParamMap[ap->paramID] = paramMap[ap->paramID]->load();
        }
        if (auto* ap = dynamic_cast<juce::AudioParameterChoice*>(param))
        {
            paramMap[ap->paramID] = apvts.getRawParameterValue(ap->paramID);
            oldParamMap[ap->paramID] = paramMap[ap->paramID]->load();
        }
    }
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    int totalNumInputChannels = getTotalNumInputChannels();
    int totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; i++)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    //手动触发parameterChanged监听：因为插件窗口未打开时，automation只会自动修改apvts的参数，不触发parameterChagned监听
    for (auto& param : getParamMap())
    {
        juce::String paramID = param.first;
        std::atomic<float>* currentValuePtr = param.second;

        float newValue = *currentValuePtr;
        float oldValue = oldParamMap[paramID];
        float diff = std::abs(newValue - oldValue);
        // 发生变化则触发监听，监听方更新所有变化值和后续逻辑
        if (diff > 0.0001f)
        {
            // sendChangeMessage();
            oldParamMap[paramID] = newValue;
            parameterChanged(paramID, newValue);
            break;
        }
    }

    pulsarSynthEngine.processSample(buffer, midiMessages, getPlayHead());
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    // return true; // (change this to false if you choose to not supply an editor)
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    //自动扫描生成控件
    // return new juce::GenericAudioProcessorEditor(*this);
    return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    //将apvts状态存入内存中，daw退出时会回调该方法进行存储，之后打开才能恢复
    //用ableton测试时，如果用VST3 redebug启动，不会走入这个方法，除非退出前自己保存工程，再次打开才能恢复状态
    juce::ignoreUnused(destData);
    std::unique_ptr<XmlElement> xml(apvts.state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    //读取内存中的apvts信息，恢复preset
    juce::ignoreUnused(data, sizeInBytes);
    std::unique_ptr<XmlElement> theParams(getXmlFromBinary(data, sizeInBytes));
    if (theParams == nullptr || !theParams->hasTagName(apvts.state.getType()))
    {
        return;
    };

    apvts.state = ValueTree::fromXml(*theParams);
    //editor处理完修改状态false
    loadingPresetFlag = true;
    apvts.replaceState(juce::ValueTree::fromXml(*theParams));
    getPulsarSynthEngine().reloadSynthPreset(apvts);
    //广播通知editor恢复ui状态，在接收广播的监听中，apvts.getRawParameterValue()将会获得最新值
    //editor可能尚未创建或已销毁，如第一次打开daw尚未打开窗口时，一旦创建就会触发，所以数据类更新放到processor中不要依赖editor
    //比如preset选了sample impulse就要立即加载，而不用等到editor创建
    sendChangeMessage();
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor*JUCE_CALLTYPE

createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

// void AudioPluginAudioProcessor::valueTreePropertyChanged(juce::ValueTree& tree,
//                                                          const juce::Identifier& property
// )
// {
//     //拿不到当前修改的参数的id，只是一个顶层tree角度来判断是否change的监听
//     pulsar.updateParameters(apvts);
//     // pulsar2.initParameters(apvts);
// }


juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    //param设置的parameter name会展示在ableton展开插件的小界面中，作为文案，而不是打开插件窗口的名字
    //输出增益：db
    params.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::outputGain, 1), "Output Gain", -60.0, 6.0, 0.0)
    );

    //播放模式
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(why::ParameterID::playMode, 1),
        "Play Mode",
        why::getPlayModeArray(),
        0 // 默认选择 index，combobox的id不为0，但index是从0开始算
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(why::ParameterID::bpm, 1), "Bpm", 20, 300, 120
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(why::ParameterID::impulseSwitch, 1),
        "Impulse Switch",
        why::getImpulseSwitchArray(),
        0 // 默认选择 index，combobox的id不为0，但这里是从0开始算
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(why::ParameterID::impulseTemplateFile, 1),
        "Template",
        BinaryResourceSingleton::getInstance().getFileNameArray(),
        0
    ));

    //train长度
    params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::trainLen, 1), "Train Period", 1, 32, 1.0)
    );
    //train duty cycle: how many numbers of pulsar period
    params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::trainDutyCycleLen, 1), "Train Duty Cycle", 1, 640, 0)
    );
    //train silence：个数，改为缩放
    params.push_back(
        std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::trainSilenceLen, 1), "Train Silence", 0, 640, 0)
    );

    //masking
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(why::ParameterID::maskOption, 1),
        "Mask Mode",
        why::getMaskOptionArray(),
        0 // 默认选择 index
    ));

    //欧几里得节奏划分pattern（train duty cycle）
    params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::euclidSteps, 1), "Euclid Steps", 1, 16, 1)
    );
    params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::euclidHits, 1), "Euclid Hits", 1, 16, 1)
    );

    //pulsarWaveform
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarWaveform, 1), "Pulsar Waveform", 0.0, 1.0, 0.0)
    );

    //pulsar duty cycle len
    params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(why::ParameterID::pulsarDutyCycleClusterLen, 1), "Pulsar Duty Cycle", 1, 32, 1)
    );
    //pulsar duty cycle ratio，比例值
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarDutyCycleRatio, 1), "Pulsar Duty Cycle Ratio", 0.01, 1.0, 0.5)
    );

    //modulation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::ampLfoWaveform, 1), "AM Waveform", 0.0, 1.0, 0.0)
    );
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::formantFreqLfoWaveform, 1), "FM Waveform", 0.0, 1.0, 0.0)
    );

    //envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarAttack, 1), "Pulsar Attack", 0.0, 1.0, 0.0001)
    );
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarDecay, 1), "Pulsar Decay", 0.0, 1.0, 0.0001)
    );
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarSustain, 1), "Pulsar Sustain", 0.01, 1.0, 1.0)
    );
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(why::ParameterID::pulsarRelease, 1), "Pulsar Release", 0.0, 1.0, 0.0001)
    );

    return {params.begin(), params.end()};
}

//不放在plugineditor，因为窗口没打开过，仍还可以操作parameter，要将主线程和UI线程解耦
void AudioPluginAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    //触发synth更新为最新状态：mapping所有parameter，property的值到synth中
    pulsarSynthEngine.executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
    {
        bool isGeneratedStochasticMaskFlag = false;
        synth->parameterChanged(apvts, parameterID, newValue, isGeneratedStochasticMaskFlag);

        // UI变更，通过广播实现，不要用setproperty，会触发propertyvalue监听，但editor不存在则调用报错，但广播则不会
        sendChangeMessage();
    });
}
