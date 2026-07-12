//
// Created by Mr. Wang on 2025/4/20.
//

#ifndef SHAREDVOICESTATE_H
#define SHAREDVOICESTATE_H
#include "Commons.h"
#include "ConvolutionResource.h"
#include "EnvelopeCanvas.h"
#include <array>

/**
 * Multiple voices share a CommonVoiceSate to represent the same fields.
 * For example, burstMask and Voices have a one-to-many relationship
 */
class CommonVoiceSate {
public:
  CommonVoiceSate() {
    // 初始化包络数据为默认值 (1.0 = 无调制)
    ampEnvelopeData.fill(1.0f);
    // 初始化 FM 包络数据为默认值 (0.0 = 无调制，表示 semitones 偏移为 0)
    fmEnvelopeData.fill(0.0f);
    // 初始化 Cluster 包络数据为默认值 (1.0 = 基础值)
    dutyCycleClusterEnvelopeData.fill(1.0f);
    // 初始化 Duty Ratio 包络数据为默认值 (0.5 = 基础值)
    dutyCycleRatioEnvelopeData.fill(0.5f);
    // 初始化 Pg Waveform 包络数据为默认值 (sine: sin(2pi*x), 范围 [-1, 1])
    for (int i = 0; i < (int)pgWaveformEnvelopeData.size(); ++i) {
      float x = static_cast<float>(i) / (pgWaveformEnvelopeData.size() - 1);
      pgWaveformEnvelopeData[i] = std::sin(2.0f * 3.14159265f * x);
    }
  }

  // whether loop playback train or not
  bool isLoop = true;

  //===========================masking===========================
  // The mask is only for the train duty cycle. For example, for two pulsar
  // periods, the output is pulse, silence, pulse, silence If the mask is
  // 101101, then output pulse, silence, pulse, pulse If there are 3 pulsar
  // periods, after being processed by this mask, pulse, silence, pulse, pulse,
  // silence, pulse will be output

  // mask option: default is off
  why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
  // burst mask: like 1001
  std::string burstMask = "";
  // euclid，like 10101 is the result of steps=5 and hits=3
  std::string euclids;

  //===========================mapping parameter of plugin processor===========================
  std::atomic<float> *bpm;

  // parameters to receive values from AudioProcessorValueTreeState，thread safe
  std::atomic<float> *outputGainParam;
  std::atomic<float> *playModeParam;
  // How many pulsar periods does the train duty cycle consist of
  std::atomic<float> *trainDutyCycleLenParam;
  // How many pulsar periods does the train interval silence consist of
  std::atomic<float> *trainSilenceParam;
  // train length：(unit: 1 beat)，4 is one bar = 4 * beats
  //  The pulsar period and fundamental frequency can be determined by train
  //  length, train dutycyle and train silence
  std::atomic<float> *trainLenParam;

  // pulsar duty cycle
  // std::atomic<float> *pulsarDutyCycleRatioParam;
  // pulsaret waveform
  std::atomic<float> *pulsarWaveformParam;

  // AM 包络数据（替代波形选择）- 2048 个 samples
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> ampEnvelopeData;
  std::atomic<float> ampEnvelopeYMin{0.01f};
  std::atomic<float> ampEnvelopeYMax{1.0f};
  std::atomic<bool> useAmpEnvelope{true}; // 是否使用包络代替 LFO

  std::atomic<float> *ampLfoDepthParam;

  // FM 包络数据（替代波形选择）- 2048 个 samples
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> fmEnvelopeData;
  std::atomic<float> fmEnvelopeYMin{-24.0f}; // 默认 0
  std::atomic<float> fmEnvelopeYMax{24.0f};  // 默认 +24 semitones
  std::atomic<bool> useFmEnvelope{true};    // 是否使用包络代替 LFO

  std::atomic<float> *formantFreqLfoDepthParam;

  // duty cycle ratio 包络数据 - 2048 个 samples
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> dutyCycleRatioEnvelopeData;
  std::atomic<float> dutyCycleRatioEnvelopeYMin{0.01f};
  std::atomic<float> dutyCycleRatioEnvelopeYMax{1.0f};
  std::atomic<bool> useDutyCycleRatioEnvelope{true};

  std::atomic<float> *dutyCycleRatioDepthParam;

  // duty cycle Cluster 包络数据 - 2048 个 samples
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> dutyCycleClusterEnvelopeData;
  std::atomic<float> dutyCycleClusterEnvelopeYMin{1.0f};
  std::atomic<float> dutyCycleClusterEnvelopeYMax{16.0f};
  std::atomic<bool> useDutyCycleClusterEnvelope{true};

  // Pg Waveform 包络数据 - 2048 个 samples，Y轴范围 [-1, 1]，用户绘制的一个周期波形
  std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> pgWaveformEnvelopeData;
  std::atomic<bool> usePgWaveformEnvelope{true};

  std::atomic<float> *dutyCycleClusterDepthParam;

  // Adsr: applied to the single final pulse(notice!silence can be converted to
  // pulse) (if cluster>0, then it still is applied to the whole pulse not
  // subdivision)
  std::atomic<float> *attackParam;
  std::atomic<float> *decayParam;
  std::atomic<float> *sustainParam;
  std::atomic<float> *releaseParam;
  std::atomic<float> *maskOptionParam;

  // generate euclid rhythm to use as a mask, like a special burst mask
  std::atomic<float> *euclidStepsParam;
  std::atomic<float> *euclidHitsParam;

  // stochastic mask: only relate to train duty cycle, = trainDutyCycle * 2
  std::string stochasticMaskStr = "";

  // mask menu option
  std::atomic<float> *impulseSwitchParam;

  // parameters to receive values from AudioProcessorValueTreeState，thread safe
  std::atomic<float> *grainSizeParam;
  std::atomic<float> *grainWetParam;

  //===========================Convolution===========================
  // store impulse file data, initialize this field when initializing synth.
  // All synth and all voices share a ConvolutionResource, so use shared_ptr
  std::shared_ptr<ConvolutionResource> convolutionResource;

  //=========pulse buffer: batch process samples and then can perform convolution with impulse response at one time=========
  juce::AudioBuffer<float> pulseBuffer;

  // 累积整个 train duty cycle 的 pulsar 输出，作为较长的 IR 传给
  // processSampleSourceWithPulsarIr，使卷积效果更明显
  juce::AudioBuffer<float> pulsarIrAccumulatorBuffer;
  int pulsarIrAccWritePos = 0;
  int pulsarIrAccTargetSize = 0;

  juce::AudioBuffer<float> pulsarLinearIrBuffer;

  bool pulsarIrReady = false;
  int pulsarIrAccumulatedSamples = 0;
  int irReloadCounter = 0;

  // Multiple voices may generate and modify the stochastic mask simultaneously.
  // To ensure their sequential execution, and they are generated only after the
  // train duty cycle changes
  std::mutex strMutex;
  float previousTrainDutyCycleLen4GenStocMask;

  // only be triggered when initializing synth and in parameterChanged (where
  // the train dutycycle length has been changed)
  void generateStochasticMask() {
    // multiple voices can update stochastic in MIDI play mode
    std::lock_guard<std::mutex> guard(strMutex);
    // generate random mask, 它的长度是train duty cycle（即pulsar
    // period个数）长度的两倍
    int totalPeriodNum = trainDutyCycleLenParam->load();
    if (previousTrainDutyCycleLen4GenStocMask == totalPeriodNum) {
      return;
    }
    // limit the max length
    while (totalPeriodNum > 64) {
      totalPeriodNum = totalPeriodNum / 2;
    }
    int num = static_cast<int>(2 * totalPeriodNum);
    stochasticMaskStr = why::generateBinaryString(num);

    previousTrainDutyCycleLen4GenStocMask = totalPeriodNum;
  };

  //===========================TODO
  // 使用采样作为pulsaret===========================
  std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
  float readHead = 0;
};

/**
 * train config will be updated every block
 **/
struct SnapShot {
public:
  // 当前使用的train config
  float bpm;

  float trainDutyCycleLenParam;
  float trainSilenceParam;
  float trainLenParam;

  float trainLenBlock;
  float trainTime;
  float trainPeriodTime;
  float trainDutyCycleTime;
  float trainSilenceTime;
  float pulsarPeriodTime;
  float fundamentalFreq; // 发射频率

  float dutyCycleRatio;
  float dutyCycleCluster;
  float dutyCycleTime;
  float pulsarSilenceTime;
  
  float pulsarDutyCycleSamples;
  float pulsarIntraSilenceSamples;
  float trainDutyCycleSamples;
  float interTrainSilenceSamples;
};
#endif // SHAREDVOICESTATE_H

