// //
// // Created by Mr. Wang on 2025/4/20.
// //
// #pragma once
// #include "JuceHeader.h"

// /**
//  * The pulsaret waveform singleton maintains only one instance globally
//  * only read without writing, guranting thread safety
//  */
// class PulsaretWaveformSingleton {
// public:
//   static PulsaretWaveformSingleton &getInstance() {
//     static PulsaretWaveformSingleton instance;
//     return instance;
//   }
  
//   // Remove the copy constructor and assignment operator to ensure that only a
//   // unique instance can be obtained
//   PulsaretWaveformSingleton(const PulsaretWaveformSingleton &) = delete;
//   PulsaretWaveformSingleton &operator=(const PulsaretWaveformSingleton &) = delete;

//   /**
//    * calculate smooth modulation between the waveforms table
//    * @param originalSampleIndex float index:0.0f-1.0f
//    * @param phase phase
//    * @return get the smooth modulation
//    */
//   float calcSample(float originalSampleIndex, float phase) {
//     // 将slider值映射到waveformLUTs数组的两个相邻波形之间
//     int numWaveforms = waveformLUTs.size();
//     float index = originalSampleIndex * (numWaveforms - 1);

//     int lowerIndex = static_cast<int>(index);                    // 选择下一个波形的索引
//     int upperIndex = std::min(lowerIndex + 1, numWaveforms - 1); // 选择上一个波形的索引，确保不越界

//     // 计算插值因子
//     float interpolationFactor = index - lowerIndex;

//     // 获取对应位置的两个波形
//     juce::dsp::LookupTableTransform<float> &lowerWaveform = *waveformLUTs[lowerIndex];
//     juce::dsp::LookupTableTransform<float> &upperWaveform = *waveformLUTs[upperIndex];

//     float s = lowerWaveform.processSample(phase) + (upperWaveform.processSample(phase) - lowerWaveform.processSample(phase)) * interpolationFactor;
//     s = juce::jlimit(-1.0f, 1.0f, s);
//     return s;
//   }

// private:
//   // Private constructor to ensure that instances cannot be created externally
//   PulsaretWaveformSingleton() { initializeWaveforms(); }

//   // The reason for using LookupTableTransform instead of juce::dsp::Oscillator
//   // because I need control the phase
//   juce::dsp::LookupTableTransform<float> sineLUT;
//   juce::dsp::LookupTableTransform<float> triangleLUT;
//   juce::dsp::LookupTableTransform<float> softSawLUT;
//   juce::dsp::LookupTableTransform<float> sawLUT;
//   juce::dsp::LookupTableTransform<float> pwmLUT;
//   juce::dsp::LookupTableTransform<float> squareLUT;
//   juce::dsp::LookupTableTransform<float> smoothRandLUT;
//   juce::dsp::LookupTableTransform<float> steppedRandLUT;

//   // pulsaret waveform
//   std::vector<juce::dsp::LookupTableTransform<float> *> waveformLUTs;

//   /**
//    * init waveform
//    * @param lut lookup对象
//    * @param waveformFunc 应用的波形函数
//    */
//   // phase is [0, 1]: 0 = start of cycle, 1 = end of cycle
//   void initializeWaveform(juce::dsp::LookupTableTransform<float> &lut, std::function<float(float)> waveformFunc) { lut.initialise(waveformFunc, 0.0f, 1.0f, 4096); }

//   // Waveform order: smooth -> harsh (classic synth sequence)
//   // 0 Sine          - perfectly smooth, no harmonics above fundamental
//   // 1 Triangle      - odd harmonics only, soft, no discontinuity
//   // 2 SoftSaw       - tanh-shaped saw, rounded edges, warm
//   // 3 Sawtooth      - full harmonic series, sharp rising edge
//   // 4 PWM (25%)     - pulse wave, asymmetric, nasal timbre
//   // 5 Square        - odd harmonics, hard discontinuity
//   // 6 SmoothRand    - interpolated random, smooth but unpredictable
//   // 7 SteppedRand   - sample-and-hold random, hardest / most abrupt
//   void initializeWaveforms() {
//     // x ∈ [0,1]: one complete cycle per unit
//     initializeWaveform(sineLUT, [](float x) { return std::sin(2.0f * juce::MathConstants<float>::pi * x); });

//     initializeWaveform(triangleLUT, [](float x) { return 1.0f - 4.0f * std::abs(x - std::floor(x + 0.5f)); });

//     initializeWaveform(softSawLUT, [](float x) {
//       float saw = 2.0f * x - 1.0f;
//       return std::tanh(2.5f * saw);
//     });

//     initializeWaveform(sawLUT, [](float x) { return 2.0f * x - 1.0f; });

//     initializeWaveform(pwmLUT, [](float x) { return (x < 0.25f) ? 1.0f : -1.0f; });

//     initializeWaveform(squareLUT, [](float x) { return (x < 0.5f) ? 1.0f : -1.0f; });

//     initializeWaveform(smoothRandLUT, [](float x) {
//       static float prev = 0.0f;
//       static float next = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//       if (x < 0.01f) {
//         prev = next;
//         next = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//       }
//       return juce::jmap(x, 0.0f, 1.0f, prev, next);
//     });

//     initializeWaveform(steppedRandLUT, [](float x) {
//       constexpr int N = 16;
//       (void)(static_cast<int>(std::floor(x * N)));
//       return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//     });

//     waveformLUTs.push_back(&sineLUT);
//     waveformLUTs.push_back(&triangleLUT);
//     waveformLUTs.push_back(&softSawLUT);
//     waveformLUTs.push_back(&sawLUT);
//     waveformLUTs.push_back(&pwmLUT);
//     waveformLUTs.push_back(&squareLUT);
//     waveformLUTs.push_back(&smoothRandLUT);
//     waveformLUTs.push_back(&steppedRandLUT);
//   }
// };
