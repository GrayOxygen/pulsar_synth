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

class PulsarSynth
{
private:
    //当前pulse状态
    why::StateEnum currentState = why::StateEnum::IntraSilence;

    //=========train=========
    float trainPeriodTime = 0.0f;
    //实际发送脉冲的时长
    float trainDutyCycleTime = 0.0f;
    //train之间的silence时长，秒
    float trainSilenceTime = 0.0f;
    //已走完的train数
    int trainCounter = 0;
    //tain结束后是否循环
    bool isLoop = true;

    //当前状态已处理的sample个数
    int sinceLastTransitionSamples = 0.0f;
    //当前状态所占全部samples个数
    int currentStateDurationSamples = 0.0;
    //在单个train中最新位置（单位：samples），每次进入到新train则会重新初始化
    int trainPositionSamples = 0.0;

    //=========burst masking=========
    //比如111010表示pulse pulse pulse silence pulse silence
    //    std::string burstMask = "1001";
    std::string burstMask = "";

    //=========pulsaret=========
    //发送了几个pulsar，可以用于控制pulse masking，只在满足指定的位置，播放pulse
    int pulsarCounter = 0;
    //从pulse到silence，或从silence到pulse都计数一次
    int pulsarChangeCounter = 0;
    //fundamental frequency of pulsar emitter，可以计算出一个周期的时长
    float fundamentalFreq = 0.0;
    //ratio = pulse length/(pulse length+silence length)
    float pulsarDutyCycleRatio = 0.5f;
    float pulsarPeriodTime = 0.0f;
    float pulsarSilence = 0.0f;

    float pulsaretPhase = 0.0f;

    //表示几个pulsar duty cycle的长度
    int trainDutyCycleLen;
    int trainSilenceLen;
    std::string euclids;
    double sampleRate = 44100.0;

    float bpm = 120;
    //默认1/64 拍
    double trainLenBlock;
    //默认1个bar
    double trainTime;

    //振荡器:自己控制phase
    juce::dsp::LookupTableTransform<float> sineLUT  ;
    juce::dsp::LookupTableTransform<float> sawLUT ;
    juce::dsp::LookupTableTransform<float> squareLUT ;
    juce::dsp::LookupTableTransform<float> triangleLUT = juce::dsp::LookupTableTransform<float>();
    juce::dsp::LookupTableTransform<float> noiseLUT = juce::dsp::LookupTableTransform<float>();
    juce::dsp::LookupTableTransform<float> pwmLUT = juce::dsp::LookupTableTransform<float>();
    juce::dsp::LookupTableTransform<float> complexWaveLUT = juce::dsp::LookupTableTransform<float>();
    //pulsaret waveform
    std::vector<juce::dsp::LookupTableTransform<float>*> waveformLUTs; // 使用unique_ptr存储多个波形表

    //lfo modulation
    juce::dsp::Oscillator<float> ampLfoSine = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> ampLfoSaw = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> ampLfoSquare = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> ampLfoTriangle = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> formantFreqLfoSine = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> formantFreqLfoSaw = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> formantFreqLfoSquare = juce::dsp::Oscillator<float>();
    juce::dsp::Oscillator<float> formantFreqLfoTriangle = juce::dsp::Oscillator<float>();

    std::vector<juce::dsp::Oscillator<float>*> ampLfos;
    std::vector<juce::dsp::Oscillator<float>*> formantLfos;

    //parameters to receive values from AudioProcessorValueTreeState
    std::atomic<float>* trainDutyCycleLenParam;

    std::atomic<float>* trainSilenceParam;
    std::atomic<float>* pulsarDutyCycleRatioParam;
    std::atomic<float>* pulsarWaveformParam;

    std::atomic<float>* formantFreqLfoParam;
    std::atomic<float>* ampLfoParam;

    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;
    std::atomic<float>* maskOptionParam;

    //1 beat为单位，4表示1个bar
    std::atomic<float>* trainLenParam;
    std::atomic<float>* outputGainParam;

    //一个pulse区域可以设置为多个pulse，长度以原本的pulse width为单位
    std::atomic<float>* pulsarDutyCycleClusterLenParam;

    //欧几里得节奏，也相当于一种mask
    std::atomic<float>* euclidStepsParam;
    std::atomic<float>* euclidHitsParam;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;

    float readHead = 0;

    why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
    std::string stochasticMaskStr;

    bool currentCacluatedPulse;

    bool impulseSwitch;

public:
    [[nodiscard]] bool& is_impulse_switch()
    {
        return impulseSwitch;
    }

    void set_impulse_switch(bool impulse_switch)
    {
        impulseSwitch = impulse_switch;
    }

    void setSampleRate(double sampleRate);

    void setPulsarSilence(float dutyCylce);

    /**
     * 设置脉冲串持续时间：freq比fundamentalFreq要小，才能装下pulsar到train上
     * @param durationLen 一个train中包含的pulse个数(1就是一个pulsar period的长度，2就是2个)
     * @param intervalSilenceLen train与train之间间隔的silence长度（1就是一个pulsar period长度，2就是2个）
     * @param isLoop 是否循环播放train
     */
    void setTrainAndPulsarDutyCycle(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);
    void buildTrain();

    /**
     * 初始化所有步骤
     * @param sampleRate
     * @param sampleBuffer
     * @param audio_play_head
     */
    void init(float sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
              juce::AudioPlayHead* audio_play_head);

    /**
     * 初始化波形
     */
    void initWaves();

    void generateRandomMask();

    /**
     * 初始化参数
     * @param apvts
     */
    void connectParameters(juce::AudioProcessorValueTreeState& apvts);

    /**
     * 将juce维护的参数映射为synth的参数
     * @param apvts
     */
    void mappingParams(const juce::AudioProcessorValueTreeState& apvts);

    void parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID);

    /**
     * 刷新adsr（应用在单个pulse上）
     * @param pulsarTime
     */
    void refreshAdsr(float pulsarTime);
    void mask(bool& passFlag, bool& existMask);

    //刚启动为什么会卡一下：debug在开始时会卡一下，run没问题
    float processSample();

    float calcActualPulse(float pulsarModFreq);

    //将slider值映射到数组中的两个相邻波形之间
    float calcFormantLfoInterpolation();

    float calcAmpLfoInterpolation();

    float calcSample();

    [[nodiscard]] bool isCurrentCacluatedPulse() const
    {
        return currentCacluatedPulse;
    }

    [[nodiscard]] std::atomic<float>*& get_train_duty_cycle_len_param()
    {
        return trainDutyCycleLenParam;
    }

    void set_train_duty_cycle_len_param(std::atomic<float>* train_duty_cycle_len_param)
    {
        trainDutyCycleLenParam = train_duty_cycle_len_param;
    }

    [[nodiscard]] std::string& get_stochastic_mask_str()
    {
        return stochasticMaskStr;
    }

    void set_stochastic_mask_str(const std::string& stochastic_mask_str)
    {
        stochasticMaskStr = stochastic_mask_str;
    }

    float get_output_gain();

    void refresh_burst_mask(juce::String burstMask);


    int build_bpm(juce::AudioPlayHead* audioPlayHead)
    {
        //不在DAW中运行，则为NULL
        if (auto* temp = audioPlayHead)
        {
            juce::AudioPlayHead::CurrentPositionInfo posInfo;
            if (audioPlayHead->getCurrentPosition(posInfo) && bpm != posInfo.bpm)
            {
                bpm = posInfo.bpm;
                return 1;
            }
        }
        return 0;
    }

    void refresh_bpm(juce::AudioPlayHead* audioPlayHead)
    {
        int r = build_bpm(audioPlayHead);
        //不在DAW中运行，则为NULL
        if (1 == r)
        {
            buildTrain();
        }
    }
};
