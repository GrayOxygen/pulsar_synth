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
 * Custom synthesizers, with different play modes corresponding to different synths
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
     * When switching the play mode, turn off all sounds.
     * The auto mode will be played in the playback of the next daw, and the midi mode will be played in the next note
     */
    void triggerSoundOffWhenSwitchPlayMode(bool closeFlag)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            juce::SynthesiserVoice* voice = getVoice(i);
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->setSoundOffWhenSwitchPlayMode(closeFlag);
        }
    }

    /**
     * Modify the bpm of the plugin, force refresh the bpm and rebuild the train, because when the bpm changes,
     * the train also changes
     *
     * @param bpm The bpm of the plugin has nothing to do with that of the daw
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
     * Initialize trian. Each voice has a separate train, and the voice also points to the same CommonVoiceState,
     * sharing some common data
     *
     * @param sampleRate sample rate
     * @param sampleBuffer  sample buffer，TODO Not used now, but retained (planned for implementing sample as pulsaret)
     * @param audioPlayHead obtain information such as the playback position
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
     * Parameter change, update voice:
     *
     * When AudioProcessorValueTreeState parameterChanged listening is after the callback, the method will be triggered
     *
     * @param apvts tree state
     * @param parameterID  parameter id
     * @param newValue up-to-date value
     * @param isGeneratedStochasticMask Whether the stochasticMask was generated, this arg will be updated when func end
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
     * Refresh the voice according to the preset
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
     * Update the burstMask of all voices, but the voices all point to the same CommonVoiceSate,
     * so only one voice needs to be updated
     *
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
     * get stochastic mask for display
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
     * In auto mode, the renderNextBlock does not follow the renderNextBlock of juce's synthesizer
     * because there is no midi trigger. So write it here.
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
            PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
            pulsarVoice->renderNextBlockDirectly(buffer, audioPlayHead, start, numSamples, currentPlayModeEnum);
        }
    }

private:
    //which play mode is
    why::PlayModeEnum myPlayModeEnum;
};
