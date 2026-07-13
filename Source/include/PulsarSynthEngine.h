//
// Created by Wang on 2025/4/24.
//
#ifndef PULSARSYNTHENGINE_H
#define PULSARSYNTHENGINE_H
#include "Commons.h"
#include "PulsarSynth.h"
#include "PulsarSynthSound.h"

/**
 * pulsar synth engine that owns the single pulsar synth instance.
 * Only DAW-transport (Auto) triggering is supported.
 */
class PulsarSynthEngine {
public:
  PulsarSynthEngine() {}

  /**
   * Obtain the synth currently in use
   * @return the pulsar synth
   */
  std::shared_ptr<PulsarSynth> &getCurrentPulsarSynth() { return pulsarSynth; }

  [[nodiscard]] why::PlayModeEnum &getCurrentPlayModeEnum() { return currentPlayModeEnum; }

  /**
   * Set the current playback mode and pause all sounds each time you set it
   *
   * @param apvts value tree
   * @param index play mode enum value
   */
  void setCurrentPlayModeEnum(juce::AudioProcessorValueTreeState &apvts, int index) {
    stopTheWorld();
    if (index == static_cast<int>(why::PlayModeEnum::Auto)) {
      this->currentPlayModeEnum = why::PlayModeEnum::Auto;
    }
    if (index == static_cast<int>(why::PlayModeEnum::NotSelected)) {
      this->currentPlayModeEnum = why::PlayModeEnum::NotSelected;
    }
    if (apvts.state.getProperty(why::PropertyID::currentPlayModeEnum) != juce::String(static_cast<int>(this->currentPlayModeEnum))) {
      apvts.state.setProperty(why::PropertyID::currentPlayModeEnum, static_cast<int>(this->currentPlayModeEnum), nullptr);
    }
  }

  /**
   * Stop all sounds: the user needs to start DAW transport again to play
   */
  void stopTheWorld() { pulsarSynth->triggerSoundOffWhenSwitchPlayMode(true); }

  /**
   * In the currently used synth, execute the specified method
   * @param func the target func
   */
  void executeCurSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth> &)> &func) {
    if (currentPlayModeEnum == why::PlayModeEnum::Auto) {
      func(pulsarSynth);
    }
  }

  /**
   * refresh the synth according to the preset, load files, etc sample
   * file一开始就一定会加载好，而template impulse file 等待使用时再加载
   * @param apvts tree state
   */
  void reloadSynthPreset(juce::AudioProcessorValueTreeState &apvts) {
    // refresh and load impulse file,
    int impulseSwitchIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseSwitch));

    // update current play mode
    int currentPlayModeIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::playMode));
    setCurrentPlayModeEnum(apvts, currentPlayModeIndex);

    // update to the latest state of synth: like mapping the values of all
    // parameters and properties to synth, etc
    executeCurSynthCallback([&](std::shared_ptr<PulsarSynth> &synth) { synth->reloadPreset(apvts); });
  }

  /**
   * Execute the specified method of all synth
   *
   * @param func The methods that need to be executed
   */
  void executeAllSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth> &)> &func) {
    for (std::shared_ptr<PulsarSynth> &synth : pulsarSynths) {
      func(synth);
    }
  }

  /**
   * init synth, sound, voice； 目前已改回只有一个synth
   * @param apvts value tree state
   */
  void init(juce::AudioProcessorValueTreeState &apvts) {
    // Prevent prepareToPlay from triggering updates multiple times to achieve idempotence
    if (!commonVoiceSate) {
      this->commonVoiceSate = std::make_shared<CommonVoiceSate>();
    }

    pulsarSynth->addSound(new PulsarSynthSound());
    pulsarSynth->addVoice(new PulsarSynthVoice());

    PulsarSynthVoice *v = dynamic_cast<PulsarSynthVoice *>(pulsarSynth->getVoice(0));
    v->setCommonVoiceSate(commonVoiceSate);
    v->mappingParams(apvts);

    pulsarSynths.push_back(pulsarSynth);
  }

  /**
   * init    train
   * @param sampleRate sample rate
   * @param samplesPerBlock samples per block
   * @param numChannels num channels
   * @param playHead audio play head to get play position info
   */
  void buildTrain(double sampleRate, int samplesPerBlock, int numChannels, juce::AudioPlayHead *playHead) {
    pulsarSynth->setCurrentPlaybackSampleRate(sampleRate);
    pulsarSynth->initTrain(sampleRate, sampleBuffer, playHead);
  }

  /**
   * the entry of processing sample
   *
   * @param buffer audio buffer
   * @param midiMessages midi message
   * @param playHead audio play head
   */
  void processSample(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages, juce::AudioPlayHead *playHead) {
    juce::ignoreUnused(midiMessages);
    if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto) {
      pulsarSynth->renderNextBlockDirectly(buffer, playHead, 0, buffer.getNumSamples());
    }
  }

private:
  // Single synth, DAW-transport (Auto) triggered
  std::shared_ptr<PulsarSynth> pulsarSynth = std::make_shared<PulsarSynth>(why::PlayModeEnum::Auto);
  std::vector<std::shared_ptr<PulsarSynth>> pulsarSynths;

  // 当前触发播放的方式
  why::PlayModeEnum currentPlayModeEnum;

  // All voices under the synth share one commonVoiceSate
  std::shared_ptr<CommonVoiceSate> commonVoiceSate;

  // Reserved buffer placeholder for sample-as-pulsaret (not used yet)
  std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
};

#endif // PULSARSYNTHENGINE_H
