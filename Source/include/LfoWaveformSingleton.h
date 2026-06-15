// //
// // Created by Mr. Wang on 2025/4/20.
// //
// #pragma once
// #include "JuceHeader.h"

// /**
//  * The lfo waveform singleton maintains only one set of instances globally for
//  * the same set of sample rate and frequency parameters
//  */
// class LfoWaveformSingleton {
// public:
//   // get singleton instance
//   static LfoWaveformSingleton &getInstance(double sampleRate, float frequency) {
//     static LfoWaveformSingleton instance(sampleRate, frequency);
//     instance.updateFrequencyIfNeeded(frequency);
//     return instance;
//   }

//   // Remove the copy constructor and assignment operator to ensure only a unique
//   // instance can be obtained
//   LfoWaveformSingleton(const LfoWaveformSingleton &) = delete;
//   LfoWaveformSingleton &operator=(const LfoWaveformSingleton &) = delete;

//   // return each LFOs for external use
//   std::vector<juce::dsp::Oscillator<float> *> &getAmpLfos() { return ampLfos; }
//   std::vector<juce::dsp::Oscillator<float> *> &getFormantLfos() {
//     return formantLfos;
//   }

//   /**
//    * calcuate sample after applying AM modulation, to implement the smooth
//    * movement between different waveforms, that means when originalIndex stay
//    * between two indexs then it features two waveform sound
//    *
//    * @param originalIndex the smooth index, 0.0f - 1.0f
//    * @param phase phase in waveform
//    * @return
//    */
//   float calcSampleAfterAM(float originalIndex, float /*phase*/) {
//     int size = getAmpLfos().size();
//     float index = originalIndex * (size - 1);
//     int lowerIndex = static_cast<int>(index);
//     int upperIndex = std::min(lowerIndex + 1, size - 1);
//     float factor = index - lowerIndex;
//     // input=0 so output is purely the LFO waveform value (no additive input)
//     float lowerVal = getAmpLfos()[lowerIndex]->processSample(0.0f);
//     float upperVal = getAmpLfos()[upperIndex]->processSample(0.0f);
//     float lfoValue = lowerVal + (upperVal - lowerVal) * factor;
//     lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);
//     // Map LFO [-1,1] to AM gain [0,1]: when lfo=-1 -> gain=0, lfo=1 -> gain=1
//     return (lfoValue + 1.0f) * 0.5f;
//   }

//   /**
//    * calcuate sample after applying FM modulation, to implement the smooth
//    * movement between different waveforms, that means when originalIndex stay
//    * between two indexs then it features two waveform sound
//    *
//    * @param originalIndex the smooth index, 0.0f - 1.0f
//    * @param phase phase in waveform
//    * @return
//    */
//   float calcSampleAfterFM(float originalIndex, float /*phase*/) {
//     int size = formantLfos.size();
//     float index = originalIndex * (size - 1);
//     int lowerIndex = static_cast<int>(index);
//     int upperIndex = std::min(lowerIndex + 1, size - 1);
//     float factor = index - lowerIndex;
//     // input=0 so output is purely the LFO waveform value (no additive input)
//     float lowerVal = getFormantLfos()[lowerIndex]->processSample(0.0f);
//     float upperVal = getFormantLfos()[upperIndex]->processSample(0.0f);
//     float lfoValue = lowerVal + (upperVal - lowerVal) * factor;
//     lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);
//     // Return [-1,1] for FM: caller uses this as frequency modulation offset
//     return lfoValue;
//   }

// private:
//   void updateFrequencyIfNeeded(float newFreq) {
//     if (std::abs(newFreq - freq) < 0.001f)
//       return;
//     freq = newFreq;
//     for (auto *osc : ampLfos)
//       osc->setFrequency(newFreq);
//     for (auto *osc : formantLfos)
//       osc->setFrequency(newFreq);
//   }

//   // Private constructor to ensure that instances cannot be created externally
//   LfoWaveformSingleton(double sampleRate, float freq) {
//     this->sampleRate = sampleRate;
//     this->freq = freq;
//     initializeWaveforms(sampleRate, freq);
//   }

//   /**
//    * init different waveforms
//    * @param sampleRate sample rate
//    * @param theFreq frequency
//    */
//   void initializeWaveforms(double sampleRate, float theFreq) {
//     // 波形生成逻辑
//     auto makeTriangle = [](float x) {
//       float normX = x / juce::MathConstants<float>::twoPi;
//       return 2.0f * std::abs(2.0f * (normX - std::floor(normX + 0.5f))) - 1.0f;
//     };

//     auto makeSaw = [](float x) {
//       return juce::jmap(x, 0.0f, juce::MathConstants<float>::twoPi, -1.0f,
//                         1.0f);
//     };

//     auto makeSquare = [](float x) {
//       return (x < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
//     };

//     auto makeSine = [](float x) { return std::sin(x); };

//     auto makeComplexWave = [](float x) { return std::sin(x); };

//     auto makeRoundedTriangle = [](float x) {
//       return 2.0f * std::abs(std::sin(juce::MathConstants<float>::pi * x)) -
//              1.0f;
//     };

//     auto makeSoftSquare = [](float x) {
//       return std::tanh(3.0f *
//                        std::sin(2.0f * juce::MathConstants<float>::pi * x));
//     };

//     auto makePwm = [](float x) { return (x < 0.25f) ? 1.0f : -1.0f; };

//     auto makeSmoothRand = [](float x) {
//       // 平滑过渡的随机波形，适合 LFO morphing（伪 Perlin 可替代）
//       static float prev = 0.0f;
//       static float next =
//           juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//       float frac = x - std::floor(x);
//       if (frac < 0.01f) {
//         prev = next;
//         next = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//       }
//       return juce::jmap(frac, 0.0f, 1.0f, prev, next); // 线性插值
//     };

//     auto makeNoise = [](float x) {
//       return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
//     };

//     auto makeStepRand = [](float x) {
//       constexpr int N = 128; // N 是步数，决定了波形的分辨率
//       int index = std::floor(x * N);
//       return juce::Random::getSystemRandom().nextFloat() * 2.0f -
//              1.0f; // 随机值范围 -1 到 1
//     };

//     // 初始化 Amp LFOs
//     ampLfoSine.prepare({sampleRate, 512, 1});
//     ampLfoSine.setFrequency(theFreq);
//     ampLfoSine.initialise(makeSine);

//     ampLfoComplexWave.prepare({sampleRate, 512, 1});
//     ampLfoComplexWave.setFrequency(theFreq);
//     ampLfoComplexWave.initialise(makeComplexWave);

//     ampLfoRoundedTriangle.prepare({sampleRate, 512, 1});
//     ampLfoRoundedTriangle.setFrequency(theFreq);
//     ampLfoRoundedTriangle.initialise(makeRoundedTriangle);

//     ampLfoTriangle.prepare({sampleRate, 512, 1});
//     ampLfoTriangle.setFrequency(theFreq);
//     ampLfoTriangle.initialise(makeTriangle);

//     ampLfoSoftSquare.prepare({sampleRate, 512, 1});
//     ampLfoSoftSquare.setFrequency(theFreq);
//     ampLfoSoftSquare.initialise(makeSoftSquare);

//     ampLfoPwm.prepare({sampleRate, 512, 1});
//     ampLfoPwm.setFrequency(theFreq);
//     ampLfoPwm.initialise(makePwm);

//     ampLfoSquare.prepare({sampleRate, 512, 1});
//     ampLfoSquare.setFrequency(theFreq);
//     ampLfoSquare.initialise(makeSquare);

//     ampLfoSaw.prepare({sampleRate, 512, 1});
//     ampLfoSaw.setFrequency(theFreq);
//     ampLfoSaw.initialise(makeSaw);

//     ampLfoSmoothRand.prepare({sampleRate, 512, 1});
//     ampLfoSmoothRand.setFrequency(theFreq);
//     ampLfoSmoothRand.initialise(makeSmoothRand);

//     ampLfoNoise.prepare({sampleRate, 512, 1});
//     ampLfoNoise.setFrequency(theFreq);
//     ampLfoNoise.initialise(makeNoise);

//     ampLfoSteppedRand.prepare({sampleRate, 512, 1});
//     ampLfoSteppedRand.setFrequency(theFreq);
//     ampLfoSteppedRand.initialise(makeStepRand);

//     // push_back的顺序对应了index的0.0f到1.0f值映射关系
//     ampLfos.push_back(&ampLfoSine);
//     ampLfos.push_back(&ampLfoComplexWave);
//     ampLfos.push_back(&ampLfoRoundedTriangle);
//     ampLfos.push_back(&ampLfoTriangle);
//     ampLfos.push_back(&ampLfoSaw);
//     ampLfos.push_back(&ampLfoSoftSquare);
//     ampLfos.push_back(&ampLfoPwm);
//     ampLfos.push_back(&ampLfoSquare);
//     ampLfos.push_back(&ampLfoSmoothRand);
//     ampLfos.push_back(&ampLfoSteppedRand);
//     // ampLfos.push_back(&ampLfoNoise);

//     // 初始化 Formant Freq LFOs
//     formantFreqSine.prepare({sampleRate, 512, 1});
//     formantFreqSine.setFrequency(theFreq);
//     formantFreqSine.initialise(makeSine);

//     formantFreqComplexWave.prepare({sampleRate, 512, 1});
//     formantFreqComplexWave.setFrequency(theFreq);
//     formantFreqComplexWave.initialise(makeComplexWave);

//     formantFreqRoundedTriangle.prepare({sampleRate, 512, 1});
//     formantFreqRoundedTriangle.setFrequency(theFreq);
//     formantFreqRoundedTriangle.initialise(makeRoundedTriangle);

//     formantFreqTriangle.prepare({sampleRate, 512, 1});
//     formantFreqTriangle.setFrequency(theFreq);
//     formantFreqTriangle.initialise(makeTriangle);

//     formantFreqSoftSquare.prepare({sampleRate, 512, 1});
//     formantFreqSoftSquare.setFrequency(theFreq);
//     formantFreqSoftSquare.initialise(makeSoftSquare);

//     formantFreqPwm.prepare({sampleRate, 512, 1});
//     formantFreqPwm.setFrequency(theFreq);
//     formantFreqPwm.initialise(makePwm);

//     formantFreqSquare.prepare({sampleRate, 512, 1});
//     formantFreqSquare.setFrequency(theFreq);
//     formantFreqSquare.initialise(makeSquare);

//     formantFreqSaw.prepare({sampleRate, 512, 1});
//     formantFreqSaw.setFrequency(theFreq);
//     formantFreqSaw.initialise(makeSaw);

//     formantFreqSmoothRand.prepare({sampleRate, 512, 1});
//     formantFreqSmoothRand.setFrequency(theFreq);
//     formantFreqSmoothRand.initialise(makeSmoothRand);

//     formantFreqNoise.prepare({sampleRate, 512, 1});
//     formantFreqNoise.setFrequency(theFreq);
//     formantFreqNoise.initialise(makeNoise);

//     formantFreqSteppedRand.prepare({sampleRate, 512, 1});
//     formantFreqSteppedRand.setFrequency(theFreq);
//     formantFreqSteppedRand.initialise(makeStepRand);

//     // push_back的顺序对应了index的0.0f到1.0f值映射关系
//     formantLfos.push_back(&formantFreqSine);
//     formantLfos.push_back(&formantFreqComplexWave);
//     formantLfos.push_back(&formantFreqRoundedTriangle);
//     formantLfos.push_back(&formantFreqTriangle);
//     formantLfos.push_back(&formantFreqSaw);
//     formantLfos.push_back(&formantFreqSoftSquare);
//     formantLfos.push_back(&formantFreqPwm);
//     formantLfos.push_back(&formantFreqSquare);
//     formantLfos.push_back(&formantFreqSmoothRand);
//     formantLfos.push_back(&formantFreqSteppedRand);
//     // formantLfos.push_back(&formantFreqNoise);
//   }

//   float freq;
//   double sampleRate;
//   juce::dsp::Oscillator<float> ampLfoSine;
//   juce::dsp::Oscillator<float> ampLfoComplexWave;
//   juce::dsp::Oscillator<float> ampLfoSoftSquare;
//   juce::dsp::Oscillator<float> ampLfoRoundedTriangle;
//   juce::dsp::Oscillator<float> ampLfoPwm;
//   juce::dsp::Oscillator<float> ampLfoTriangle;
//   juce::dsp::Oscillator<float> ampLfoSquare;
//   juce::dsp::Oscillator<float> ampLfoSaw;
//   juce::dsp::Oscillator<float> ampLfoSmoothRand;
//   juce::dsp::Oscillator<float> ampLfoNoise;
//   juce::dsp::Oscillator<float> ampLfoSteppedRand;

//   juce::dsp::Oscillator<float> formantFreqSine;
//   juce::dsp::Oscillator<float> formantFreqComplexWave;
//   juce::dsp::Oscillator<float> formantFreqSoftSquare;
//   juce::dsp::Oscillator<float> formantFreqRoundedTriangle;
//   juce::dsp::Oscillator<float> formantFreqPwm;
//   juce::dsp::Oscillator<float> formantFreqTriangle;
//   juce::dsp::Oscillator<float> formantFreqSquare;
//   juce::dsp::Oscillator<float> formantFreqSaw;
//   juce::dsp::Oscillator<float> formantFreqSmoothRand;
//   juce::dsp::Oscillator<float> formantFreqNoise;
//   juce::dsp::Oscillator<float> formantFreqSteppedRand;

//   std::vector<juce::dsp::Oscillator<float> *> ampLfos;
//   std::vector<juce::dsp::Oscillator<float> *> formantLfos;
// };
