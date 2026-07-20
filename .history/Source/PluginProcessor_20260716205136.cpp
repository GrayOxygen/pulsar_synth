#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "include/BinaryResourceSingleton.h"
#include "include/PulsarSynth.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      apvts(*this, nullptr, "ParamTree", createParameterLayout()) {
  // valuetree listener
  // apvts.state.addListener(*this);

  // Add apvts listeners for all parameters
  for (int i = 0; i < apvts.state.getNumChildren(); ++i) {
    auto child = apvts.state.getChild(i);
    if (child.hasType("PARAM") && child.hasProperty("id")) {
      juce::String paramID = child["id"];
      apvts.addParameterListener(paramID, this);
    }
  }

  // init synth engine
  pulsarSynthEngine.init(apvts);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
  for (int i = 0; i < apvts.state.getNumChildren(); ++i) {
    juce::ValueTree child = apvts.state.getChild(i);
    if (child.hasType("PARAM") && child.hasProperty("id")) {
      juce::String paramID = child["id"];
      apvts.removeParameterListener(paramID, this);
    }
  }
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1; // NB: some hosts don't cope very well if you tell them there are 0
            // programs,
  // so this should be at least 1, even if you're not really implementing
  // programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String &newName) { juce::ignoreUnused(index, newName); }

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  // Use this method as the place to do any pre-playback initialisation that you
  // need..
  juce::ignoreUnused(sampleRate, samplesPerBlock);
  why::sampleRate = sampleRate;

  // 配置limiter
  juce::dsp::ProcessSpec spec;
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
  spec.numChannels = getTotalNumOutputChannels();

  // 初始化 Limiter
  limiter.prepare(spec);

  // 设置 Limiter 参数
  limiter.setThreshold(-0.1f); // 阈值设为 -0.1 dB，留出一点安全余量（Headroom）
  limiter.setRelease(100.0f);  // 释放时间设为 100ms，让声音平滑过渡

  // 初始化train
  pulsarSynthEngine.buildTrain(sampleRate, samplesPerBlock, getTotalNumOutputChannels(), getPlayHead());

  // 设置高通滤波器参数：20Hz 截止频率，Q值为 0.707
  highPassFilter.prepare(spec);

  // 使用公共 API 设置系数（ProcessorDuplicator 通过 *state 共享系数给所有声道）
  *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 30.0f, 2.0f);
}

void AudioPluginAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ignoreUnused(midiMessages);

  juce::ScopedNoDenormals noDenormals;
  int totalNumInputChannels = getTotalNumInputChannels();
  int totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; i++) {
    buffer.clear(i, 0, buffer.getNumSamples());
  }

  pulsarSynthEngine.processSample(buffer, midiMessages, getPlayHead());

  // limit & dc filter
  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::ProcessContextReplacing<float> context(block);

  // 对音频块应用高通滤波
  // highPassFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
  // limiter.process(context);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
  // return true; // (change this to false if you choose to not supply an editor)
  return true;
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
  //  return new juce::GenericAudioProcessorEditor(*this);
  // create custom editor
  return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  // *** save apvts in memory - when daw exits, this method will be called to
  // store the state, and when opened again, the state will be restored when
  // using VST3 with ableton, if redebug is used, this method will not be
  // called, unless the project is saved before exiting, and then opened again
  // ***
  juce::ignoreUnused(destData);
  std::unique_ptr<juce::XmlElement> xml(apvts.state.createXml());
  copyXmlToBinary(*xml, destData);
}
/**
 * 设置状态信息 （重新打开上一次打开的参数设置）
 * @param data 状态数据
 * @param sizeInBytes 状态数据大小
 */
void AudioPluginAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
  // read apvts from memory, and recover preset
  juce::ignoreUnused(data, sizeInBytes);
  std::unique_ptr<juce::XmlElement> theParams(getXmlFromBinary(data, sizeInBytes));
  if (theParams == nullptr || !theParams->hasTagName(apvts.state.getType())) {
    return;
  };

  apvts.state = juce::ValueTree::fromXml(*theParams);
  // editor处理完修改状态false
  loadingPresetFlag = true;
  // 更新参数到apvts中
  apvts.replaceState(juce::ValueTree::fromXml(*theParams));
  // 刷新synth状态
  getPulsarSynthEngine().reloadSynthPreset(apvts);
  // 广播通知editor恢复ui状态：如preset选了sample
  // impulse就要立即加载，而不用等到editor创建
  // 此时，editor可能尚未创建或已销毁，如第一次打开daw尚未打开窗口时，而editor使用listener来接收，则会因找不到而报错，所以选择广播
  // 另外，数据类更新放到processor中，editor处理ui变动，实现解耦是最佳实践
  sendChangeMessage();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE

createPluginFilter() {
  return new AudioPluginAudioProcessor();
}

// void AudioPluginAudioProcessor::valueTreePropertyChanged(juce::ValueTree&
// tree,
//                                                          const
//                                                          juce::Identifier&
//                                                          property
// )
// {
//     //拿不到当前修改的参数的id，只是一个顶层tree角度来判断是否change的监听
//     pulsar.updateParameters(apvts);
//     // pulsar2.initParameters(apvts);
// }

/**
 * 创建audio parameter
 * @return
 */
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
  // 在ableton中，param设置的parameter name，会在Device Parameters
  // window中展示（未打开插件窗口） output gain
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::outputGain, 1), "Output Gain", -60.0, 6.0, 0.0));

  // bpm
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::bpm, 1), "Bpm", 20, 300, 120));

  // impulse
  params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(why::ParameterID::impulseSwitch, 1), "Impulse Switch", why::getImpulseSwitchArray(), 0));

  params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(why::ParameterID::impulseTemplateFile, 1), "Template", BinaryResourceSingleton::getInstance().getFileNameArray(), 0));

  // train
  params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(why::ParameterID::trainLen, 1), "Train Period", 1, 8, 1.0));
  params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(why::ParameterID::trainDutyCycleLen, 1), "Train Duty Cycle", 1, 64, 0));
  params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(why::ParameterID::trainSilenceLen, 1), "Train Silence", 0, 64, 0));

  // masking
  params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(why::ParameterID::maskOption, 1), "Mask Mode", why::getMaskOptionArray(), 0));

  // 欧几里得节奏划分pattern（只应用于train duty cycle）
  params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(why::ParameterID::euclidSteps, 1), "Euclid Steps", 1, 16, 1));
  params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(why::ParameterID::euclidHits, 1), "Euclid Hits", 1, 16, 1));

  //   modulation depth
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::amEnvelopeDepth, 1), "AM Env Depth", 0.0, 1.0, 0.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::formantFreqLfoDepth, 1), "FM Env Depth", 0.01, 1.0, 0.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::dutyCycleRatioDepth, 1), "Duty Cycle Ratio Depth", 0.01, 1.0, 1.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::dutyCycleClusterDepth, 1), "Duty Cycle Cluster Depth", 0.01, 1.0, 1.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::fmLfoDepth, 1), "FM LFO Depth", 0.01, 1.0, 1.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::amLfoDepth, 1), "AM LFO Depth", 0.01, 1.0, 1.0));

  // envelope
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::pulsarAttack, 1), "Pulsar Attack", 0.0, 1.0, 0.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::pulsarDecay, 1), "Pulsar Decay", 0.0, 1.0, 0.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::pulsarSustain, 1), "Pulsar Sustain", 0.01, 1.0, 1.0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(why::ParameterID::pulsarRelease, 1), "Pulsar Release", 0.0, 1.0, 0.0));

  return {params.begin(), params.end()};
}

/**
 * 监听控件参数变化，注意：
 * 1，getRawParameterValue() or getParameter() methods is not guaranteed to return the up-to-date value but newValue is
 *
 * 不放在plugineditor，因为窗口没打开过，仍还可以操作parameter，要将主线程和UI线程解耦
 * @param parameterID parameter id
 * @param newValue up-to-date value
 */
void AudioPluginAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue) {
  auto currentTime = juce::Time::getCurrentTime().toMilliseconds();

  // 限制每隔20ms处理一次参数变化 limit every parameterchanged invoke within 10ms only once
  if (currentTime - lastChangeTime > 10) {
    lastChangeTime = currentTime;

    // 触发synth更新为最新状态：mapping所有parameter，property的值到synth中
    pulsarSynthEngine.executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) {
      bool isGeneratedStochasticMaskFlag = false;
      synth->parameterChanged(apvts, parameterID, newValue, isGeneratedStochasticMaskFlag);

      // UI变更，通过广播实现，不要用setproperty，会触发propertyvalue监听，但editor不存在则调用报错，但广播则不会
      sendChangeMessage();
    });
  }

  // Notice!!!: Only the currently set impulse file will be used as impulse
  // response. This option can be modified in the plugin window, but the
  // automation cannot be changed to avoid possible frequent operations, which
  // may lead to a large number of IO operations and consume performance.
  // Therefore, a trade-off has been made here.
}