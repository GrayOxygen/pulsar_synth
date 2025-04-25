//
// Created by Wang on 2025/4/24.
//
#ifndef PULSARSYNTHENGINE_H
#define PULSARSYNTHENGINE_H
#include "Commons.h"
#include "PulsarSynth.h"

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

    void setCurrentPlayModeEnum(juce::AudioProcessorValueTreeState& apvts, why::PlayModeEnum currentPlayModeEnum)
    {
        this->currentPlayModeEnum = currentPlayModeEnum;
        apvts.state.setProperty(why::PropertyID::currentPlayModeEnum,
                                juce::String(static_cast<int>(currentPlayModeEnum)), nullptr);
    }

    //如果需要修改synth的状态，则统一修改，共享一套声音状态
    std::vector<std::shared_ptr<PulsarSynth>>& getPulsarSynths()
    {
        return pulsarSynths;
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

    void executeEachPulsarSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        for (std::shared_ptr<PulsarSynth>& synth : pulsarSynths)
        {
            func(synth);
        }
    }

    // 在切换模式时，采用共享voice的模式
    void changePlayMode(why::PlayModeEnum destPlayModeEnum)
    {
        // if (currentPlayModeEnum == why::PlayModeEnum::Auto && why::PlayModeEnum::Midi == destPlayModeEnum)
        // {
        //     int voiceNum = pulsarSynthForMidi->getNumVoices();
        //     pulsarSynthForMidi->clearVoices();
        //     for (int i = 0; i < voiceNum; i++)
        //     {
        //         SynthesiserVoice* voice = pulsarSynthForAuto->getVoice(0);
        //         PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
        //         pulsarSynthForAuto.cop
        //         pulsarSynthForMidi->addVoice(pulsarVoice);
        //     };
        // }
        // if (currentPlayModeEnum == why::PlayModeEnum::Midi && why::PlayModeEnum::Auto == destPlayModeEnum)
        // {
        //     int voiceNum = pulsarSynthForAuto->getNumVoices();
        //     pulsarSynthForAuto->clearVoices();
        //     for (int i = 0; i < voiceNum; i++)
        //     {
        //         SynthesiserVoice* voice = pulsarSynthForMidi->getVoice(i);
        //         PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
        //         pulsarSynthForAuto->addVoice(new PulsarSynthVoice(pulsarSynthForMidi->getVoice(0)));
        //     };
        // }
    }

    void initConvolution(double sampleRate, int samplesPerBlock, int numChannels)
    {
        this->convolutionResource = std::make_shared<ConvolutionResource>(sampleRate, samplesPerBlock, numChannels);
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
        std::shared_ptr<CommonVoiceSate> commonVoiceSate = std::make_shared<CommonVoiceSate>();

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
        // pulsarSynthForMidi->initConvolution(sampleRate, samplesPerBlock, numChannels);
        // pulsarSynthForAuto->initConvolution(sampleRate, samplesPerBlock, numChannels);

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
            // AudioPlayHead::CurrentPositionInfo pos;
            // if (getPlayHead() != nullptr && getPlayHead()->getCurrentPosition(pos))
            // {
            //     bool isNowPlaying = pos.isPlaying;
            //
            //     if (isNowPlaying && !wasPlayingLastFrame)
            //     {
            //         // Transport just started
            //         isActive = true;
            //     }
            //
            //     if (!isNowPlaying && wasPlayingLastFrame)
            //     {
            //         // Transport just stopped
            //         isActive = false;
            //     }
            //
            //     wasPlayingLastFrame = isNowPlaying;
            // }
            //
            // if (isActive)
            // {
            //     float bpm = 120.0f;
            //     float freq = bpm / 60.0f; // 2 Hz
            //     float phaseInc = freq / getSampleRate();
            //     float beatSamples = (60.0 / bpm) * getSampleRate();
            //     for (int i = 0; i < buffer.getNumSamples(); i++)
            //     {
            //         if (std::fmod((double)beatCounter, beatSamples) < 1.0)
            //         {
            //             pulsaretPhase = 0.0f;
            //         }
            //         float s = std::sin(2.0f * juce::MathConstants<float>::pi * pulsaretPhase);
            //         pulsaretPhase += phaseInc;
            //         if (pulsaretPhase > 1.0f)
            //             pulsaretPhase -= 1.0f;
            //
            //         left[i] = s;
            //         right[i] = s;
            //         beatCounter++;
            //     }
            // }
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

    //TODO  准备废弃
    //TODO 存储采样的buffer，准备废弃
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
};


#endif //PULSARSYNTHENGINE_H
