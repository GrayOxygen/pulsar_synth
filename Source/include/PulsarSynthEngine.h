//
// Created by Wang on 2025/4/24.
//
#ifndef PULSARSYNTHENGINE_H
#define PULSARSYNTHENGINE_H
#include <ResourceSingleton.h>
#include "Commons.h"
#include "PulsarSynth.h"
#include "PulsarSynthSound.h"

class PulsarSynthEngine
{
public:
    PulsarSynthEngine()
    {
    }

    std::shared_ptr<PulsarSynth>& getCurrentPulsarSynth(juce::AudioProcessorValueTreeState& apvts)
    {
        int mode = static_cast<int>(apvts.getRawParameterValue("playMode")->load());
        if (mode == static_cast<int>(why::PlayModeEnum::Auto))
        {
            return pulsarSynthForAuto;
        }
        if (mode == static_cast<int>(why::PlayModeEnum::Midi))
        {
            return pulsarSynthForMidi;
        }
        return pulsarSynthForAuto;
    }

    [[nodiscard]] why::PlayModeEnum& getCurrentPlayModeEnum()
    {
        return currentPlayModeEnum;
    }

    void setCurrentPlayModeEnum(juce::AudioProcessorValueTreeState& apvts, int index)
    {
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
        apvts.state.setProperty(why::PropertyID::currentPlayModeEnum,
                                juce::String(static_cast<int>(this->currentPlayModeEnum)), nullptr);
    }

    //如果需要修改synth的状态，则统一修改，共享一套声音状态
    std::vector<std::shared_ptr<PulsarSynth>>& getPulsarSynths()
    {
        return pulsarSynths;
    }

    //切换模式转换，就让用户重新触发，midi模式会自动随着下一个音符播放而触发，auto模式需要用户再次点击播放
    void stopTheWorld()
    {
        pulsarSynthForMidi->allNotesOff(0, false);
        pulsarSynthForAuto->triggerSoundOffWhenSwitchPlayMode();
    }

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
        //必须直接加载到convolution resource，否则在impulse file菜单切换时，不会再去获取数据源
        String sampleImpulsePath = apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString();
        //展示最新的sample impulse选项展示，并直接加载文件到convolution resource
        //更新sample impulse path展示
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

        if (!apvts.state.getProperty(why::PropertyID::currentPlayModeEnum).isVoid())
        {
            int currentPlayModeEnumInt = apvts.state.getProperty(why::PropertyID::currentPlayModeEnum).toString().
                                               getIntValue();
            setCurrentPlayModeEnum(apvts, currentPlayModeEnumInt);
        }

        //触发synth更新为最新状态：mapping所有parameter，property的值到synth中
        executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->reloadPreset(apvts);
        });
    }


    void executeAllSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        for (std::shared_ptr<PulsarSynth>& synth : pulsarSynths)
        {
            func(synth);
        }
    }

    void initConvolution(double sampleRate, int samplesPerBlock, int numChannels)
    {
        //防止prepareToPlay多次触发更新
        if (!convolutionResource)
        {
            this->convolutionResource = std::make_shared<ConvolutionResource>(sampleRate, samplesPerBlock, numChannels);
        }
        for (std::shared_ptr<PulsarSynth> tempSynth : pulsarSynths)
        {
            for (int i = 0; i < tempSynth->getNumVoices(); ++i)
            {
                juce::SynthesiserVoice* voice = tempSynth->getVoice(i);
                // 将其转换为自定义的 PulsarSynthVoice
                PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
                pulsarVoice->getCommonVoiceSate()->convolutionResource = convolutionResource;
            }
        }
    }

    void init(juce::AudioProcessorValueTreeState& apvts)
    {
        //防止prepareToPlay多次触发更新
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

    void buildTrain(double sampleRate, int samplesPerBlock, int numChannels, juce::AudioPlayHead* playHead)
    {
        pulsarSynthForMidi->setCurrentPlaybackSampleRate(sampleRate);
        pulsarSynthForAuto->setCurrentPlaybackSampleRate(sampleRate);

        this->initConvolution(sampleRate, samplesPerBlock, numChannels);

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

    void processSample(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, juce::AudioPlayHead* playHead)
    {
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto)
        {
            pulsarSynthForAuto->renderNextBlockDirectly(buffer, playHead, midiMessages, 0, buffer.getNumSamples(),
                                                        getCurrentPlayModeEnum());
        }
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Midi)
        {
            //更新bpm，重新调整train，速度只由插件控制
            // pulsarSynthForMidi->refreshBpm(getPlayHead());
            pulsarSynthForMidi->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
        }
    }

    //获取convolution resource的统一入口
    [[nodiscard]] std::shared_ptr<ConvolutionResource>& getConvolutionResource()
    {
        return convolutionResource;
    }

private:
    //两种播放模式下的synth，对用户来说只是一个
    std::shared_ptr<PulsarSynth> pulsarSynthForMidi = std::make_unique<PulsarSynth>(why::PlayModeEnum::Midi);
    std::shared_ptr<PulsarSynth> pulsarSynthForAuto = std::make_unique<PulsarSynth>(why::PlayModeEnum::Auto);
    std::vector<std::shared_ptr<PulsarSynth>> pulsarSynths;

    //一个pluginprocessor对应一个track；复制track时，这里的playmode必须要设置为对应值
    //当前触发播放的方式：实际上所有synth都遵守同一个播放状态
    why::PlayModeEnum currentPlayModeEnum;

    //所有synth，所有voice，共享一个impulse source
    std::shared_ptr<ConvolutionResource> convolutionResource;
    //一份共同属性
    std::shared_ptr<CommonVoiceSate> commonVoiceSate;


    //TODO  准备废弃
    //TODO 存储采样的buffer，准备废弃
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
};


#endif //PULSARSYNTHENGINE_H
