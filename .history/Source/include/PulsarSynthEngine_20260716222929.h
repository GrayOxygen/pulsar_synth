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

  /**
   * Stop all sounds: the user needs to start DAW transport again to play
   */
  void stopTheWorld() { pulsarSynth->triggerSoundOffWhenSwitchPlayMode(true); }

  /**
   * In the currently used synth, execute the specified method
   * @param func the target func
   */
  void executeCurSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth> &)> &func) { func(pulsarSynth); }

  /**
   * refresh the synth according to the preset, load files, etc sample
   * file一开始就一定会加载好，而template impulse file 等待使用时再加载
   * @param apvts tree state
   */
  void reloadSynthPreset(juce::AudioProcessorValueTreeState &apvts) {
    // refresh and load impulse file,
    int impulseSwitchIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseSwitch));

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
    pulsarSynth->initTrain(sampleRate, playHead);
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
    pulsarSynth->renderNextBlockDirectly(buffer, playHead, 0, buffer.getNumSamples());
  }

private:
  // Single synth, DAW-transport (Auto) triggered
  std::shared_ptr<PulsarSynth> pulsarSynth = std::make_shared<PulsarSynth>();
  std::vector<std::shared_ptr<PulsarSynth>> pulsarSynths;

  // All voices under the synth share one commonVoiceSate
  std::shared_ptr<CommonVoiceSate> commonVoiceSate;
};

#endif // PULSARSYNTHENGINE_H
