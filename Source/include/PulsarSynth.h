//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include "Commons.h"
#include "PulsarSynthVoice.h"
#include "Commons.h"
#include "ConvolutionResource.h"
#include "PluginProcessor.h"

//自定义合成器入口，放所有自定义的方法
class PulsarSynth : public juce::Synthesiser
{
public:
    PulsarSynth()
    {
    };

    PulsarSynth(why::PlayModeEnum myPlayModeEnum)
    {
        this->myPlayModeEnum = myPlayModeEnum;
    };

    [[nodiscard]] std::shared_ptr<ConvolutionResource>& getConvolutionResource()
    {
        return convolutionResource;
    }

    void setConvolutionResource(const std::shared_ptr<ConvolutionResource>& convolutionResource)
    {
        this->convolutionResource = convolutionResource;
    }

    [[nodiscard]] why::PlayModeEnum& getMyPlayModeEnum()
    {
        return myPlayModeEnum;
    }

    void setMyPlayModeEnum(why::PlayModeEnum myPlayModeEnum)
    {
        this->myPlayModeEnum = myPlayModeEnum;
    }

    // //废弃：DAW中，修改bpm，同时更新train配置，速度都从插件里控制，精简些
    // void refreshBpm(juce::AudioPlayHead* audioPlayHead)
    // {
    //     for (int i = 0; i < getNumVoices(); ++i)
    //     {
    //         juce::SynthesiserVoice* voice = getVoice(i);
    //         // 将其转换为自定义的 PulsarSynthVoice
    //         PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
    //
    //         int r = pulsarVoice->refreshBpm(audioPlayHead);
    //         //不在DAW中运行，则为NULL
    //         if (1 == r)
    //         {
    //             pulsarVoice->changeToNewTrainAfterPulsarPeriodOrTrainEnd(TODO);
    //         }
    //     }
    // }

    //与DAW的速度无关，改了插件的速度直接修改synth基于的bpm值，会直接改变train
    void forceRefreshBpm(float bpm)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            bool bpmChangedFlag = pulsarVoice->updateBpmDirectly(bpm);
            pulsarVoice->changeToNewTrainAfterPulsarPeriodOrTrainEnd(bpmChangedFlag);
        }
    }

    //初始化voice
    void initTrain(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                   juce::AudioPlayHead* audio_play_head)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->initSynth(sampleRate, sampleBuffer, audio_play_head);
        }
    }

    void initConvolution(double sampleRate, int samplesPerBlock, int numChannels)
    {
        convolutionResource = std::make_shared<ConvolutionResource>(sampleRate, samplesPerBlock, numChannels);
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->setConvolutionResource(convolutionResource);
        }
    }

    void parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID, float newValue,
                          bool& isGeneratedStochasticMask)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);;
            pulsarVoice->parameterChanged(apvts, parameterID, newValue, isGeneratedStochasticMask);
            //UI变更，生成stochastic mask时要展示出来
        }
    }

    void reloadPreset(juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->reloadPreset(apvts);
        }
    }

    void refreshBurstMask(juce::String burstMask)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->refreshBurstMask(burstMask);
        }
    }

    std::string getStochasticMaskStr()
    {
        std::string result;
        //每个voice都有自己的stochastic mask str，因为频率不同，该值也会不同，仅以第一个voice的值作为展示
        if (getNumVoices() <= 0)
        {
            result = std::string("");
            return result;
        }
        juce::SynthesiserVoice* voice = getVoice(0);
        PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
        result = pulsarVoice->getStochasticMaskStr();
        return result;
    }

    //play mode为auto时直接计算sample
    void renderNextBlockDirectly(juce::AudioBuffer<float>& buffer, juce::AudioPlayHead* audioPlayHead,
                                 const juce::MidiBuffer& midiBuffer, int start,
                                 int numSamples, why::PlayModeEnum currentPlayModeEnum)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            // 将其转换为自定义的 PulsarSynthVoice
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->renderNextBlockDirectly(buffer, audioPlayHead, start, numSamples, currentPlayModeEnum);
        }
    }

private:
    //所有voice共享一个impulse source
    std::shared_ptr<ConvolutionResource> convolutionResource;
    why::PlayModeEnum myPlayModeEnum;
};
