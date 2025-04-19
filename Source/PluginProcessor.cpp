#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PulsarSynth.h"
#include "JuceHeader.h"

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
    pulsar->connectParameters(apvts);
    // //监听整个参数树 valuetree listener
    // apvts.state.addListener(this);
}


AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);

    // audioFormatManager->registerBasicFormats();

    //配置convolution
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock; // 或者直接设为 2048
    spec.numChannels = getTotalNumOutputChannels();
    convolution->prepare(spec);

    // 创建一个 JUCE String 对象，包含路径
    // juce::String path = "/Users/blueear/Documents/Samples/test dd/rim.wav";
    //
    // // 对路径进行 URL 转义
    // juce::String escapedPath = juce::URL::addEscapeChars(path, false, false);
    // // 使用转码后的路径
    // juce::File file(path);
    //
    // audioFormatReader.reset(audioFormatManager->createReaderFor(file));
    // sampleBuffer->setSize(audioFormatReader->numChannels, audioFormatReader->lengthInSamples);
    // audioFormatReader->read(sampleBuffer.get(), 0, audioFormatReader->lengthInSamples, 0, true, true);
    pulsar->init(getSampleRate(), sampleBuffer, getPlayHead());
    //VST3测试，prepareToPlay执行了两次，而init中使用了this->sineLUT，但执行一次后会std::move(sineLUT)从而导致该变量为空，下次访问就报错
    DBG("HOW MANY PREPAREPLAY" <<why::get_thread_id_str());
    // pulsar2.init(sampleBuffer);
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

    int numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    //更新bpm，重新调整train
    pulsar->refresh_bpm(getPlayHead());

    //创建一个临时的 AudioBuffer 来存放你生成的 pulse 信号
    juce::AudioBuffer<float> pulseBuffer(2, numSamples);
    juce::Array<bool> silences(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float pulse = pulsar->processSample();
        if (!pulsar->isCurrentCacluatedPulse())
        {
            silences.set(i, true);
        }
        else
        {
            silences.set(i, false);
        }
        //float pulse = std::sin(2.0f * juce::MathConstants<float>::pi * i / numSamples);
        pulseBuffer.setSample(0, i, pulse);
        pulseBuffer.setSample(1, i, pulse);
    }

    //卷积
    //初始化convolution
    float postGain = 1.0;
    if (convolution->getCurrentIRSize() > 0 && pulsar->is_impulse_switch() && shouldUseConvolution)
    {
        juce::dsp::AudioBlock<float> block(pulseBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution->process(context);
        postGain = 5.0;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        //        float pulse = pulsar2.processSample();
        // float pulse = pulsar.processSample();
        float relativeVolume = silences.indexOf(i) == true ? 0.1 : 1.0;

        leftChannel[i] += relativeVolume * pulseBuffer.getSample(0, i) * postGain * pulsar->get_output_gain();
        rightChannel[i] += relativeVolume * pulseBuffer.getSample(1, i) * postGain * pulsar->get_output_gain();
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
    //自动扫描生成控件
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
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
