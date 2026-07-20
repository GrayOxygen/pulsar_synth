//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include "Commons.h"
#include "EnvelopeCanvas.h"
#include "PulsarSynthVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>

/**
 * Custom synthesizers, with different play modes corresponding to different synths
 */
class PulsarSynth : public juce::Synthesiser {
public:
  PulsarSynth(){};
  /**
   * When switching the play mode, turn off all sounds.
   * The auto mode will be played in the playback of the next daw, and the midi mode will be played in the next note
   */
  void triggerSoundOffWhenSwitchPlayMode(bool closeFlag) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      pulsarVoice->setSoundOffWhenSwitchPlayMode(closeFlag);
    }
  }

  /**
   * Modify the bpm of the plugin, force refresh the bpm and rebuild the train, because when the bpm changes,
   * the train also changes
   *
   * @param bpm The bpm of the plugin has nothing to do with that of the daw
   */
  void forceRefreshBpmAndRebuildTrain(float bpm) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      // 将其转换为自定义的 PulsarSynthVoice
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      if (why::bpm.load() != bpm)
        why::bpm.store(bpm);
      pulsarVoice->setEnterNextTrain(true);
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
  void initTrain(double sampleRate, juce::AudioPlayHead *audioPlayHead) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
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
  void parameterChanged(juce::AudioProcessorValueTreeState &apvts, juce::String parameterID, float newValue, bool &isGeneratedStochasticMask) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      ;
      pulsarVoice->parameterChanged(apvts, parameterID, newValue, isGeneratedStochasticMask);
    }
  }

  /**
   * Refresh the voice according to the preset
   * @param apvts tree state
   */
  void reloadPreset(juce::AudioProcessorValueTreeState &apvts) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      pulsarVoice->reloadPreset(apvts);
    }
  }

  /**
   * Update the burstMask of all voices, but the voices all point to the same CommonVoiceSate,
   * so only one voice needs to be updated
   *
   * @param burstMask burst mask
   */
  void refreshBurstMask(juce::String burstMask) {
    if (getNumVoices() > 0) {
      juce::SynthesiserVoice *voice = getVoice(0);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      pulsarVoice->getCommonVoiceSate()->burstMask = burstMask.toStdString();
    }
  }

  /**
   * get stochastic mask for display
   *
   * @return stochastic mask
   */
  std::string getStochasticMaskStr() {
    std::string result;
    // auto模式下，仅包含一个voice，所以stochastic mask只有一个，midi模式下仅展示第一个voice的即可，实际上每个voice的可能不同
    if (getNumVoices() <= 0) {
      result = std::string("");
      return result;
    }

    if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
      auto state = voice->getCommonVoiceSate();
      if (state && !state->stochasticMaskStr.empty())
        return state->stochasticMaskStr;
    }
    return "";
  }

  /**
   * Set whether to use AM envelope instead of LFO waveform
   */
  void setUseAmpEnvelope(bool useEnvelope) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->useAmpEnvelope.store(useEnvelope);
      }
    }
  }

  /**
   * Set AM envelope scale
   */
  void setAmpEnvelopeScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->ampEnvelopeScale.store(scale);
      }
    }
  }

  /**
   * Set AM envelope Y axis range
   */
  void setAmpEnvelopeYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->ampEnvelopeYMin.store(yMin);
        voice->getCommonVoiceSate()->ampEnvelopeYMax.store(yMax);
      }
    }
  }

  /**
   * Set AM envelope data (2048 samples)
   */
  void setAmpEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->ampEnvelopeData = data;
      }
    }
  }

  /**
   * Get AM envelope data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getAmpEnvelopeData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->ampEnvelopeData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(1.0f);
    return defaultData;
  }

  /**
   * Set whether to use FM envelope instead of LFO waveform
   */
  void setUseFmEnvelope(bool useEnvelope) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->useFmEnvelope.store(useEnvelope);
      }
    }
  }

  /**
   * Set FM envelope scale
   */
  void setFmEnvelopeScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmEnvelopeScale.store(scale);
      }
    }
  }

  /**
   * Set FM envelope Y axis range (semitones)
   */
  void setFmEnvelopeYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmEnvelopeYMin.store(yMin);
        voice->getCommonVoiceSate()->fmEnvelopeYMax.store(yMax);
      }
    }
  }

  /**
   * Set FM envelope data (2048 samples)
   */
  void setFmEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmEnvelopeData = data;
      }
    }
  }

  /**
   * Get FM envelope data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getFmEnvelopeData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->fmEnvelopeData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(0.0f);
    return defaultData;
  }

  /**
   * Set FM lfo scale
   */
  void setFmLfoScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmLfoScale.store(scale);
      }
    }
  }

  /**
   * Set FM lfo Y axis range (semitones)
   */
  void setFmLfoYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmLfoYMin.store(yMin);
        voice->getCommonVoiceSate()->fmLfoYMax.store(yMax);
      }
    }
  }

  /**
   * Set FM lfo data (2048 samples)
   */
  void setFmLfoData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->fmLfoData = data;
      }
    }
  }

  /**
   * Get FM lfo data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getFmLfoData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->fmLfoData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(0.0f);
    return defaultData;
  }

  /**
   * Set AM lfo scale
   */
  void setAmLfoScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->amLfoScale.store(scale);
      }
    }
  }

  /**
   * Set AM lfo Y axis range (semitones)
   */
  void setAmLfoYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->amLfoYMin.store(yMin);
        voice->getCommonVoiceSate()->amLfoYMax.store(yMax);
      }
    }
  }

  /**
   * Set AM lfo data (2048 samples)
   */
  void setAmLfoData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->amLfoData = data;
      }
    }
  }

  /**
   * Get AM lfo data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getAmLfoData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->amLfoData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(0.0f);
    return defaultData;
  }

  /**
   * Set whether to use Duty Cycle Ratio envelope instead of LFO waveform
   */
  void setUseDutyCycleRatioEnvelope(bool useEnvelope) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->useDutyCycleRatioEnvelope.store(useEnvelope);
      }
    }
  }

  /**
   * Set Duty Cycle Ratio envelope scale
   */
  void setDutyCycleRatioEnvelopeScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleRatioEnvelopeScale.store(scale);
      }
    }
  }

  /**
   * Set Duty Cycle Ratio envelope Y axis range (semitones)
   */
  void setDutyCycleRatioEnvelopeYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleRatioEnvelopeYMin.store(yMin);
        voice->getCommonVoiceSate()->dutyCycleRatioEnvelopeYMax.store(yMax);
      }
    }
  }

  /**
   * Set Duty Cycle Ratio envelope data (2048 samples)
   */
  void setDutyCycleRatioEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleRatioEnvelopeData = data;
        // voice->setEnterNextTrain(true);
      }
    }
  }

  /**
   * Get Duty Cycle Ratio envelope data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getDutyCycleRatioEnvelopeData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->dutyCycleRatioEnvelopeData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(0.01f);
    return defaultData;
  }

  /**
   * Set whether to use Duty Cycle Cluster envelope instead of LFO waveform
   */
  void setUseDutyCycleClusterEnvelope(bool useEnvelope) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->useDutyCycleClusterEnvelope.store(useEnvelope);
      }
    }
  }

  /**
   * Set Duty Cycle Cluster envelope scale
   */
  void setDutyCycleClusterEnvelopeScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleClusterEnvelopeScale.store(scale);
      }
    }
  }

  /**
   * Set Duty Cycle Cluster envelope Y axis range (semitones)
   */
  void setDutyCycleClusterEnvelopeYRange(float yMin, float yMax) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleClusterEnvelopeYMin.store(yMin);
        voice->getCommonVoiceSate()->dutyCycleClusterEnvelopeYMax.store(yMax);
      }
    }
  }

  /**
   * Set Duty Cycle Cluster envelope data (2048 samples)
   */
  void setDutyCycleClusterEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->dutyCycleClusterEnvelopeData = data;
        // voice->setEnterNextTrain(true);
      }
    }
  }

  /**
   * Get Duty Cycle Cluster envelope data
   */
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getDutyCycleClusterEnvelopeData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->dutyCycleClusterEnvelopeData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(1.0f);
    return defaultData;
  }

  void setUsePgWaveformEnvelope(bool useEnvelope) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->usePgWaveformEnvelope.store(useEnvelope);
      }
    }
  }

  void setPgWaveformEnvelopeScale(float scale) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->pgWaveformEnvelopeScale.store(scale);
      }
    }
  }

  void setPgWaveformEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        voice->getCommonVoiceSate()->pgWaveformEnvelopeData = data;
      }
    }
  }

  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> getPgWaveformEnvelopeData() const {
    if (getNumVoices() > 0) {
      if (auto *voice = dynamic_cast<PulsarSynthVoice *>(getVoice(0))) {
        return voice->getCommonVoiceSate()->pgWaveformEnvelopeData;
      }
    }
    std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> defaultData;
    defaultData.fill(0.5f);
    return defaultData;
  }

  /**
   * In auto mode, the renderNextBlock does not follow the renderNextBlock of juce's synthesizer
   * because there is no midi trigger. So write it here.
   *
   * @param buffer audio buffer
   * @param audioPlayHead audio play head
   * @param start start index
   * @param numSamples num of samples
   */
  void renderNextBlockDirectly(juce::AudioBuffer<float> &buffer, juce::AudioPlayHead *audioPlayHead, int start, int numSamples) {
    for (int i = 0; i < getNumVoices(); ++i) {
      juce::SynthesiserVoice *voice = getVoice(i);
      PulsarSynthVoice *pulsarVoice = dynamic_cast<PulsarSynthVoice *>(voice);
      pulsarVoice->renderNextBlockDirectly(buffer, audioPlayHead, start, numSamples);
    }
  }
};
