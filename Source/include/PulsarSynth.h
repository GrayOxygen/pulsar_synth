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

/**
 * 自定义合成器，不同的play mode对应不同的synth
 */
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

    [[nodiscard]] why::PlayModeEnum& getMyPlayModeEnum()
    {
        return myPlayModeEnum;
    }

    void setMyPlayModeEnum(why::PlayModeEnum myPlayModeEnum)
    {
        this->myPlayModeEnum = myPlayModeEnum;
    }

    /**
     * 切换play mode时，关闭所有声音，auto模式将会在下次daw的playback中播放，midi模式将会在下一个note播放
     */
    void triggerSoundOffWhenSwitchPlayMode()
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->setSoundOffWhenSwitchPlayMode(true);
        }
    }

    /**
     * 修改插件的bpm，强制刷新bpm和rebuild train，因为bpm变了，train就变了
     * @param bpm 插件的bpm，与daw的bpm无关
     */
    void forceRefreshBpmAndRebuildTrain(float bpm)
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

    /**
     * 初始化trian，每个voice都单独拥有一个train，同时voice也指向同一个CommonVoiceState，共享部分公用数据
     *
     * @param sampleRate sample rate
     * @param sampleBuffer  sample buffer，暂时没用到，但保留（计划用于实现将sample作为pulsaret）
     * @param audioPlayHead 用于获取播放位置等信息
     */
    void initTrain(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                   juce::AudioPlayHead* audioPlayHead)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->initSynthVoice(sampleRate, sampleBuffer, audioPlayHead);
        }
    }

    /**
     * 参数变更，更新voice：
     *
     * 当AudioProcessorValueTreeState的parameterChanged监听被回调后，该方法也会被触发
     *
     * @param apvts tree state
     * @param parameterID  parameter id
     * @param newValue up-to-date value
     * @param isGeneratedStochasticMask 是否生成了stochastic mask的标记，true为是，false为否
     */
    void parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID, float newValue,
                          bool& isGeneratedStochasticMask)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);;
            pulsarVoice->parameterChanged(apvts, parameterID, newValue, isGeneratedStochasticMask);
        }
    }

    /**
     * 根据preset刷新voice
     * @param apvts tree state
     */
    void reloadPreset(juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->reloadPreset(apvts);
        }
    }

    /**
     * 更新所有voice的burstMask，不过voices都指向同一个CommonVoiceSate，所以只需更新一个voice的即可
     * @param burstMask burst mask
     */
    void refreshBurstMask(juce::String burstMask)
    {
        if (getNumVoices() > 0)
        {
            juce::SynthesiserVoice* voice = getVoice(0);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->getCommonVoiceSate()->burstMask = burstMask.toStdString();
        }
    }

    /**
     * 获取展示的stochastic mask
     *
     * @return stochastic mask
     */
    std::string getStochasticMaskStr()
    {
        std::string result;
        //auto模式下，仅包含一个voice，所以stochastic mask只有一个，midi模式下仅展示第一个voice的即可，实际上每个voice的可能不同
        if (getNumVoices() <= 0)
        {
            result = std::string("");
            return result;
        }

        if (auto* voice = dynamic_cast<PulsarSynthVoice*>(getVoice(0)))
        {
            auto state = voice->getCommonVoiceSate();
            if (state && !state->stochasticMaskStr.empty())
                return state->stochasticMaskStr;
        }
        return "";
    }

    /**
     * auto模式下的renderNextBlock，因为无midi触发，所以没有走juce的Synthesiser的renderNextBlock
     *
     * @param buffer audio buffer
     * @param audioPlayHead audio play head
     * @param midiBuffer midi buffer
     * @param start start index
     * @param numSamples num of samples
     * @param currentPlayModeEnum current play mode
     */
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
    //当前synth属于哪一种播放模式
    why::PlayModeEnum myPlayModeEnum;
};
