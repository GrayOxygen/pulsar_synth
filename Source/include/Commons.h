//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include "JuceHeader.h"
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

/**
 * Commons maintains some common states and simple methods
 */
namespace why { 
// use beatDivision * beat as a minimal unit
extern std::atomic<int> beatDivision;

// Define the global speed: It is the speed of the plugin rather than the speed
// of the DAW. This plugin is only affected by the speed of the plugin
extern std::atomic<float> bpm;
extern std::atomic<double> sampleRate;

// ensure the relation id(or combobox index) and resource name is one-to-one,
//  !!!There are new additions or modifications in the subsequent resource. The
//  mapping relationship can also be kept unchanged by modifying the filename
//  corresponding to the id. If the file is deleted, the file mapping of the
//  preset will inevitably become invalid. In addition, maximum compatibility is
//  guaranteed!!!
//   For example, if the initial file has a1, a3, a5, and the preset records
//   index=2, which points a3. When the plugin is upgraded and a2 is added, then
//   index=2 in the preset will point to a2.
//  However, if 1=a1, 2=a3, 3=a5, and the newly added file is 4=a2, then the
//  relationship between the name and id of the previous file has no effect at
//  all
extern std::map<int, juce::String> resourceIdToName;

/**
 * pulse state, precisely speaking, which stage the pulsar train is currently in
 */
enum class PulsarStateEnum {
  Pulse,
  IntraSilence,     // silence inside one pular period
  InterTrainSilence // silence of train interval
};

/**
 * impulse switch combobox option
 */
enum class ImpulseSwitchEnum { Off = 0, Template = 1, Sample = 2 };

/**
 * mask combobox option
 */
enum class MaskOptionEnum { Off = 0, BurstMask = 1, EuclidMask = 2, StochasticMask = 3 };

/**
 * play mode
 */
enum class PlayModeEnum {
  NotSelected = 0, // no sound
  Auto = 1         // triggered by playback of DAW
};

/**
 * audio parameter name
 */
struct ParameterID {
  static constexpr const char *outputGain = "outputGain";
  static constexpr const char *bpm = "bpm";
  static constexpr const char *impulseSwitch = "impulseSwitch";
  static constexpr const char *impulseTemplateFile = "impulseTemplateFile";
  static constexpr const char *trainLen = "trainLen";
  static constexpr const char *trainDutyCycleLen = "trainDutyCycleLen";
  static constexpr const char *trainSilenceLen = "trainSilenceLen";
  static constexpr const char *maskOption = "maskOption";
  static constexpr const char *euclidSteps = "euclidSteps";
  static constexpr const char *euclidHits = "euclidHits";
  static constexpr const char *pulsarWaveform = "pulsarWaveform";
  static constexpr const char *pulsarDutyCycleClusterLen = "pulsarDutyCycleClusterLen";
  static constexpr const char *pulsarDutyCycleRatio = "pulsarDutyCycleRatio";
  static constexpr const char *ampLfoWaveform = "ampLfoWaveform";
  static constexpr const char *formantFreqLfoWaveform = "formantFreqLfoWaveform";
  static constexpr const char *ampLfoDepth = "ampLfoDepth";
  static constexpr const char *formantFreqLfoDepth = "formantFreqLfoDepth";
  static constexpr const char *pulsarAttack = "pulsarAttack";
  static constexpr const char *pulsarDecay = "pulsarDecay";
  static constexpr const char *pulsarSustain = "pulsarSustain";
  static constexpr const char *pulsarRelease = "pulsarRelease";
  static constexpr const char *playMode = "playMode";
};

// Some UI elements cannot be bound to audio parameters, so preset and UI
// updates cannot be automated. Instead, the state is saved manually using
// properties for updates.
struct PropertyID {
  static constexpr const char *burstMask = "burstMask";
  static constexpr const char *stochasticMask = "stochasticMask";
  static constexpr const char *sampleImpulsePath = "sampleImpulsePath";
  static constexpr const char *currentPlayModeEnum = "currentPlayModeEnum";
};

/**
 * c++ enumeration does not support directly obtaining all the names of the
 * enumeration as in java, so a separate method is used to solve it
 *
 * @return impulse switch option names
 */
juce::StringArray getImpulseSwitchArray();

/**
 * c++ enumeration does not support directly obtaining all the names of the
 * enumeration as in java, so a separate method is used to solve it
 *
 * @return mask option names
 */
juce::StringArray getMaskOptionArray();

/**
 * genreate euclid rhythm
 *
 * 5,3 is 1 1 1,0 0 then 10 10 1
 * 5,2 is 10 10,0 then 10100
 * 7,3 is 10 10,0 0 0 then 100 100,0 then 1001000
 *
 * @param steps rhythm length
 * @param hits how many divides
 * @return
 */
std::string generateEuclidRhythm(int steps, int hits);

/**
 * Set a random character in the string to 1
 *
 * @param binaryString string contains only 0 and 1
 */
void setRandomIndexToOne(std::string &binaryString);

/**
 * Generate a string of the specified length that contains only 0 and 1
 *
 * @param length target length
 * @return 如00011101
 */
std::string generateBinaryString(int length);

/**
 * c++ enumeration does not support directly obtaining all the names of the
 * enumeration as in java, so a separate method is used to solve it
 *
 * @return play modes
 */
juce::StringArray getPlayModeArray();
} // namespace why
