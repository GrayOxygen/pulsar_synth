//
// Created by Mr. Wang on 2025/4/20.
//

#ifndef SHAREDVOICESTATE_H
#define SHAREDVOICESTATE_H
#include "Commons.h"
#include "ConvolutionResource.h"

/**
 * Multiple voices share a CommonVoiceSate to represent the same fields.
 * For example, burstMask and Voices have a one-to-many relationship
*/
class CommonVoiceSate
{
public:
    //whether loop playback train or not
    bool isLoop = true;

    //===========================masking===========================
    //The mask is only for the train duty cycle. For example, for two pulsar periods, the output is pulse, silence, pulse, silence
    //If the mask is 101101, then output pulse, silence, pulse, pulse
    //If there are 3 pulsar periods, after being processed by this mask, pulse, silence, pulse, pulse, silence, pulse will be output

    //mask option: default is off
    why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
    //burst mask: like 1001
    std::string burstMask = "";
    //euclid，like 10101 is the result of steps=5 and hits=3
    std::string euclids;

    //===========================mapping parameter of plugin processor===========================
    //parameters to receive values from AudioProcessorValueTreeState，thread safe
    std::atomic<float>* outputGainParam;
    std::atomic<float>* playModeParam;
    //How many pulsar periods does the train duty cycle consist of
    std::atomic<float>* trainDutyCycleLenParam;
    //How many pulsar periods does the train interval silence consist of
    std::atomic<float>* trainSilenceParam;
    //train length：(unit: 1 beat)，4 is one bar = 4 * beats
    // The pulsar period and fundamental frequency can be determined by train length, train dutycyle and train silence
    std::atomic<float>* trainLenParam;

    //pulsar duty cycle
    std::atomic<float>* pulsarDutyCycleRatioParam;
    //pulsaret waveform
    std::atomic<float>* pulsarWaveformParam;
    //A pulse can be divided into multiple pulses.
    //For example, if a train has only one pulsar period and the pulse ratio is 0.5, then the pulse duty cycle ==pulse silence
    //If the cluster is set to 4, it was originally pulse and silence, and now it is
    //(pulse, pulse, pulse, pulse)(occupying the length of the original 1 pulse), silence
    std::atomic<float>* pulsarDutyCycleClusterLenParam;

    //FM LFO: Applied to the single final pulse(notice!silence can be converted to pulse)
    //(if cluster>0, then it's still applied to the whole pulse not subdivision)
    std::atomic<float>* formantFreqLfoParam;
    //AM LFO: Applied to the single final pulse(notice!silence can be converted to pulse)
    //(if cluster>0, then it's still applied to the whole pulse not subdivision)
    std::atomic<float>* ampLfoParam;

    //Adsr: applied to the single final pulse(notice!silence can be converted to pulse)
    //(if cluster>0, then it still is applied to the whole pulse not subdivision)
    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;
    std::atomic<float>* maskOptionParam;

    //generate euclid rhythm to use as a mask, like a special burst mask
    std::atomic<float>* euclidStepsParam;
    std::atomic<float>* euclidHitsParam;

    //stochastic mask: only relate to train duty cycle, = trainDutyCycle * 2
    std::string stochasticMaskStr = "";

    //mask menu option
    std::atomic<float>* impulseSwitchParam;

    //===========================Convolution===========================
    //store impulse file data, initialize this field when initializing synth.
    //All synth and all voices share a ConvolutionResource, so use shared_ptr
    std::shared_ptr<ConvolutionResource> convolutionResource;

    //=========pulse buffer: batch process samples and then can perform convolution with impulse response at one time=========
    juce::AudioBuffer<float> pulseBuffer;

    // Multiple voices may generate and modify the stochastic mask simultaneously.
    // To ensure their sequential execution, and they are generated only after the train duty cycle changes
    std::mutex strMutex;
    float previousTrainDutyCycleLen4GenStocMask;

    //only be triggered when initializing synth and in parameterChanged (where the train dutycycle length has been changed)
    void generateStochasticMask()
    {
        //multiple voices can update stochastic in MIDI play mode
        std::lock_guard<std::mutex> guard(strMutex);
        //generate random mask, 它的长度是train duty cycle（即pulsar period个数）长度的两倍
        int totalPeriodNum = trainDutyCycleLenParam->load();
        if (previousTrainDutyCycleLen4GenStocMask == totalPeriodNum)
        {
            return;
        }
        //limit the max length
        while (totalPeriodNum > 64)
        {
            totalPeriodNum = totalPeriodNum / 2;
        }
        int num = static_cast<int>(2 * totalPeriodNum);
        stochasticMaskStr = why::generateBinaryString(num);

        previousTrainDutyCycleLen4GenStocMask = totalPeriodNum;
    };

    //===========================TODO 使用采样作为pulsaret===========================
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
    float readHead = 0;
};
#endif //SHAREDVOICESTATE_H
