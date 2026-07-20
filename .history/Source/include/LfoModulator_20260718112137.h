// //
// // Created by Mr. Wang on 2025/4/20.
// //
// #pragma once
// #include "JuceHeader.h"

// /**
//  * Per-voice LFO modulator: each voice (or plugin instance) owns its own
//  * LfoModulator so that internal phase states are independent and do not
//  * interfere across voices or plugin instances.
//  *
//  * Uses a single oscillator per modulation type (AM / FM). The waveform
//  * shape is selectable at runtime via setAmpWaveform / setFormantWaveform.
//  */
// class LfoModulator {
// public:
//   // Waveform choices exposed to the UI
//   enum Waveform { Sine = 0, Triangle, Saw, Square, NumWaveforms };

//   LfoModulator() = default;

//   /**
//    * Must be called once before use (e.g. in voice startNote or prepare).
//    * @param sampleRate audio sample rate
//    * @param frequency  initial LFO frequency
//    */
//   void prepare(double sampleRate, float frequency) {
//     this->sr = sampleRate;
//     this->freq = frequency;

//     ampLfo.prepare({sampleRate, 512, 1});
//     ampLfo.setFrequency(frequency);
//     ampLfo.initialise(getWaveformFunc(ampWaveformIndex));

//     formantLfo.prepare({sampleRate, 512, 1});
//     formantLfo.setFrequency(frequency);
//     formantLfo.initialise(getWaveformFunc(formantWaveformIndex));
//   }

//   /**
//    * Update the LFO frequency (call per-block or when pulsar freq changes).
//    */
//   void setFrequency(float newFreq) {
//     if (std::abs(newFreq - freq) < 0.001f)
//       return;
//     freq = newFreq;
//     ampLfo.setFrequency(newFreq);
//     formantLfo.setFrequency(newFreq);
//   }

//   /**
//    * Switch the AM LFO waveform shape.
//    * @param index Waveform enum value (0=Sine, 1=Triangle, 2=Saw, 3=Square)
//    */
//   void setAmpWaveform(int index) {
//     if (index == ampWaveformIndex)
//       return;
//     ampWaveformIndex = index;
//     ampLfo.initialise(getWaveformFunc(index));
//   }

//   /**
//    * Switch the FM (formant) LFO waveform shape.
//    * @param index Waveform enum value (0=Sine, 1=Triangle, 2=Saw, 3=Square)
//    */
//   void setFormantWaveform(int index) {
//     if (index == formantWaveformIndex)
//       return;
//     formantWaveformIndex = index;
//     formantLfo.initialise(getWaveformFunc(index));
//   }
  
//   /**
//    * Calculate AM modulation value.
//    * @param amount modulation depth 0.0 (no mod) to 1.0 (full mod)
//    * @return gain multiplier in [1-amount, 1]
//    */
//   float calcSampleAfterAM(float amount) {
//     float lfoValue = ampLfo.processSample(0.0f);
//     lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);
//     // Map [-1,1] to [0,1]
//     float gain = (lfoValue + 1.0f) * 0.5f;
//     // amount controls depth: 0 = no modulation (return 1), 1 = full
//     return 1.0f - amount * (1.0f - gain);
//   }

//   /**
//    * Calculate FM modulation value.
//    * @param amount modulation depth 0.0 to 1.0
//    * @return modulation offset in [-amount, +amount]
//    */
//   float calcSampleAfterFM(float amount) {
//     float lfoValue = formantLfo.processSample(0.0f);
//     lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);
//     return lfoValue * amount;
//   }

// private:
//   static std::function<float(float)> getWaveformFunc(int index) {
//     switch (index) {
//     case Sine:
//       return [](float x) { return std::sin(x); };
//     case Triangle:
//       return [](float x) {
//         float normX = x / juce::MathConstants<float>::twoPi;
//         return 2.0f * std::abs(2.0f * (normX - std::floor(normX + 0.5f))) - 1.0f;
//       };
//     case Saw:
//       return [](float x) { return juce::jmap(x, 0.0f, juce::MathConstants<float>::twoPi, -1.0f, 1.0f); };
//     case Square:
//       return [](float x) { return (x < juce::MathConstants<float>::pi) ? 1.0f : -1.0f; };
//     default:
//       return [](float x) { return std::sin(x); };
//     }
//   }

//   double sr = 0.0;
//   float freq = 0.0f;
//   int ampWaveformIndex = Sine;
//   int formantWaveformIndex = Sine;
//   juce::dsp::Oscillator<float> ampLfo;
//   juce::dsp::Oscillator<float> formantLfo;
// };
