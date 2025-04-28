//
// Created by Wang on 2025/4/24.
//
#ifndef PULSARSYNTHENGINE_H
#define PULSARSYNTHENGINE_H
#include "Commons.h"
#include "PulsarSynth.h"
#include "PulsarSynthSound.h"

/**
 *
 */
class PulsarSynthEngine
{
public:
    PulsarSynthEngine()
    {
    }

    /**
     * 获取当前正在使用的synth
     * @return 默认则返回auto模式下的synth
     */
    std::shared_ptr<PulsarSynth>& getCurrentPulsarSynth()
    {
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto)
        {
            return pulsarSynthForAuto;
        }
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Midi)
        {
            return pulsarSynthForMidi;
        }
        return pulsarSynthForAuto;
    }

    [[nodiscard]] why::PlayModeEnum& getCurrentPlayModeEnum()
    {
        return currentPlayModeEnum;
    }

    /**
     * 设置当前播放模式，每次设置都暂停所有声音
     *
     * @param apvts value tree
     * @param index play mode enum value
     */
    void setCurrentPlayModeEnum(int index)
    {
        stopTheWorld();
        if (index == static_cast<int>(why::PlayModeEnum::Auto))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::Auto;
        }
        if (index == static_cast<int>(why::PlayModeEnum::Midi))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::Midi;
        }
        if (index == static_cast<int>(why::PlayModeEnum::NotSelected))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::NotSelected;
        }
    }

    /**
     * 停止所有声音：midi模式会自动随着下一个音符播放而触发，auto模式需要用户再次点击播放
     */
    void stopTheWorld()
    {
        for (int channel = 1; channel <= 16; ++channel)
        {
            pulsarSynthForMidi->allNotesOff(channel, false);
        }
        pulsarSynthForAuto->triggerSoundOffWhenSwitchPlayMode();
    }

    /**
     * 在当前正在使用的synth中，执行指定方法
     * @param func 需要执行的方法
     */
    void executeCurSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        if (currentPlayModeEnum == why::PlayModeEnum::Auto)
        {
            func(pulsarSynthForAuto);
        }
        if (currentPlayModeEnum == why::PlayModeEnum::Midi)
        {
            func(pulsarSynthForMidi);
        }
    }

    /**
     * 根据preset刷新synth，加载文件等
     * @param apvts tree state
     */
    void reloadSynthPreset(juce::AudioProcessorValueTreeState& apvts)
    {
        //刷新并加载impulse file,
        int impulseSwitchIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseSwitch));

        //展示最新的template impulse选项展示，并加载文件到convolution resource
        int index = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseTemplateFile));

        if (impulseSwitchIndex == static_cast<int>(why::ImpulseSwitchEnum::Template))
        {
            getConvolutionResource()->saveTemplateImpulse(juce::String(index + 1));
        }

        //展示最新的sample impulse选项展示，并（必须）直接加载文件到convolution resource（切换impulse菜单选项不再保存file）
        String sampleImpulsePath = apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString();
        if (sampleImpulsePath.isNotEmpty())
        {
            //如果当前选中了sample impulse模式，则将资源文件加载为impulse
            //没加载过impulse resouce则加载一次
            juce::File file(sampleImpulsePath);
            if (file.existsAsFile())
            {
                // 文件存在，则保存资源文件到convolution resource中（全局的）
                getConvolutionResource()->saveLastSampleFileAsBlock(file);
                if (impulseSwitchIndex == static_cast<int>(why::ImpulseSwitchEnum::Sample))
                {
                    //加载impulse response
                    getConvolutionResource()->loadSampleImpulseFile();
                }
            }
        }

        //更新当前播放模式
        int currentPlayModeIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::playMode));
        setCurrentPlayModeEnum(currentPlayModeIndex);

        //触发synth更新为最新状态：比如mapping所有parameter，property的值到synth中等
        executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->reloadPreset(apvts);
        });
    }

    /**
     * 执行所有synth的指定方法
     *
     * @param func 需要执行的方法
     */
    void executeAllSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        for (std::shared_ptr<PulsarSynth>& synth : pulsarSynths)
        {
            func(synth);
        }
    }

    /**
     * 初始化convolution
     *
     * 注意：打开daw时，可能会多次触发prepareToPlay，如果有初始化操作，就要保证幂等性
     *
     * @param sampleRate sample rate
     * @param samplesPerBlock samples per block
     * @param numChannels nums of channel
     */
    void initConvolution(double sampleRate, int samplesPerBlock, int numChannels)
    {
        //防止prepareToPlay多次触发更新，保证幂等性
        if (!convolutionResource)
        {
            this->convolutionResource = std::make_shared<ConvolutionResource>(sampleRate, samplesPerBlock, numChannels);
        }

        for (std::shared_ptr<PulsarSynth> tempSynth : pulsarSynths)
        {
            for (int i = 0; i < tempSynth->getNumVoices(); ++i)
            {
                juce::SynthesiserVoice* voice = tempSynth->getVoice(i);
                PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
                pulsarVoice->getCommonVoiceSate()->convolutionResource = convolutionResource;
            }
        }
    }

    /**
     * 初始化synth, sound, voice信息
     * @param apvts value tree state
     */
    void init(juce::AudioProcessorValueTreeState& apvts)
    {
        //防止prepareToPlay多次触发更新，实现幂等性
        if (!commonVoiceSate)
        {
            this->commonVoiceSate = std::make_shared<CommonVoiceSate>();
        }

        pulsarSynthForMidi->addSound(new PulsarSynthSound());
        //不允许同时输入多个note，听觉上没意义
        for (int i = 0; i < 4; i++)
        {
            pulsarSynthForMidi->addVoice(new PulsarSynthVoice());

            PulsarSynthVoice* v = dynamic_cast<PulsarSynthVoice*>(pulsarSynthForMidi->getVoice(i));
            v->setCommonVoiceSate(commonVoiceSate);
            v->connectParameters(apvts);
        }

        pulsarSynthForAuto->addSound(new PulsarSynthSound());
        pulsarSynthForAuto->addVoice(new PulsarSynthVoice());

        PulsarSynthVoice* v = dynamic_cast<PulsarSynthVoice*>(pulsarSynthForAuto->getVoice(0));
        v->setCommonVoiceSate(commonVoiceSate);
        v->connectParameters(apvts);

        pulsarSynths.push_back(pulsarSynthForMidi);
        pulsarSynths.push_back(pulsarSynthForAuto);
    }

    /**
     * 初始化convolution，并构建train
     * @param sampleRate sample rate
     * @param samplesPerBlock samples per block
     * @param numChannels num channels
     * @param playHead audio play head，用来获取播放位置信息
     */
    void buildTrain(double sampleRate, int samplesPerBlock, int numChannels, juce::AudioPlayHead* playHead)
    {
        pulsarSynthForMidi->setCurrentPlaybackSampleRate(sampleRate);
        pulsarSynthForAuto->setCurrentPlaybackSampleRate(sampleRate);

        this->initConvolution(sampleRate, samplesPerBlock, numChannels);
        //TODO 读取采样，用于使用采样做为pulsaret waveform
        // audioFormatManager->registerBasicFormats();
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

        pulsarSynthForMidi->initTrain(sampleRate, sampleBuffer, playHead);
        pulsarSynthForAuto->initTrain(sampleRate, sampleBuffer, playHead);
    }

    /**
     * 处理sample的入口
     *
     * @param buffer audio buffer
     * @param midiMessages midi message
     * @param playHead audio play head
     */
    void processSample(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, juce::AudioPlayHead* playHead)
    {
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto)
        {
            pulsarSynthForAuto->renderNextBlockDirectly(buffer, playHead, midiMessages, 0, buffer.getNumSamples(),
                                                        getCurrentPlayModeEnum());
        }
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Midi)
        {
            pulsarSynthForMidi->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
        }
    }

    /**
     * 获取公用的convolution resource
     * @return convolution resource
     */
    [[nodiscard]] std::shared_ptr<ConvolutionResource>& getConvolutionResource()
    {
        return convolutionResource;
    }

private:
    //两种播放模式下对应着两个synth，但对用户来说只是一个
    std::shared_ptr<PulsarSynth> pulsarSynthForMidi = std::make_unique<PulsarSynth>(why::PlayModeEnum::Midi);
    std::shared_ptr<PulsarSynth> pulsarSynthForAuto = std::make_unique<PulsarSynth>(why::PlayModeEnum::Auto);
    std::vector<std::shared_ptr<PulsarSynth>> pulsarSynths;

    //一个pluginprocessor对应一个track；复制track时，这里的playmode必须要设置为对应值
    //当前触发播放的方式：实际上所有synth都遵守同一个播放状态
    why::PlayModeEnum currentPlayModeEnum;

    //所有synth下的所有voice，共享一个impulse source
    std::shared_ptr<ConvolutionResource> convolutionResource;
    //所有synth下的所有voice，共享一个commonVoiceSate
    std::shared_ptr<CommonVoiceSate> commonVoiceSate;

    //TODO  准备废弃
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
};


#endif //PULSARSYNTHENGINE_H