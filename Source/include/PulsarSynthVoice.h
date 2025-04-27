//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "Commons.h"
#include "CommonVoiceSate.h"

//在plugineditor去修改voice、sound的值要注意线程安全
class PulsarSynthVoice : public juce::SynthesiserVoice
{
private:
    //AUTO模式====================================参数 DAW PLAY, STOP control====================================
    //上一次是否处于播放状态
    bool wasPlayingLastFrame;
    //true：因为切换模式需要静音，false不需要，用来控制切换模式暂停声音，让用户再次重新播放
    bool soundOffWhenSwitchPlayMode;
    //===========================midi相关===========================
    //按下的midi键盘
    bool playing = false;
    juce::ADSR envelope;
    float currentVelocity = 1.0;
    //====================================train====================================
    //默认1/32 拍的时间
    double trainLenBlock;
    //默认1个bar
    double trainTime;

    //=======================
    //上一次的train duty cycle长度：多少个pulsar period
    int previousTrainDutyCycleNum = 0;
    //上一次的train silence长度：多少个pulsar period
    int previousTrainSilenceNum = 0;
    //上一次的train长度：多少个trainLenBlock
    int previousTrainLen = 0;

    //train周期长，秒
    float trainPeriodTime = 0.0f;
    //实际发送脉冲period的时长，秒
    float trainDutyCycleTime = 0.0f;
    //train之间的silence时长，秒
    float trainSilenceTime = 0.0f;

    //已走完的train数，非循环模式则播放一次就结束
    int trainCounter = 0;

    //=====控制DAW play stop控制train变更走向=====
    bool changeTrainTrace = false;
    bool isTheSameTrainConfig;
    int newTrainDurationLen;
    int newTrainIntervalSilenceLen;
    int newTrainLen;
    int pulsarSamples;
    int intraSilenceSamples;
    int interTrainSilenceSamples;
    int trainDutyCycleSamples;
    bool isActive = false;

    //当前pulse状态（StateEnum）所需的samples个数
    int currentStateDurationSampleNum = 0.0;
    //当前状态（StateEnum）已处理的sample个数，-1表示初始状态
    int hasPassedSampleNumInsideTrain = -999.0f;
    int totalSampleCount = 0.0f;
    int realTotalSampleCount = 0.0f;
    //在单个train中最新位置（单位：samples），每次进入到新train则会重新初始化
    int trainPositionSamples = 0.0;

    //在经过mask最终输出后，当前输出的是否是pulse（经过mask可以将原来的silence转为pulse）
    bool currentSampleInPulse;
    //当前原始的阶段标识（经过mask之前，因为经过masking后，可能出现silence变成pulse等情况）：
    why::PulsarStateEnum currentState = why::PulsarStateEnum::IntraSilence;
    //=====控制DAW play stop控制train变更走向=====

    //===========================pulsaret===========================
    //fundamental frequency of pulsar emitter，可以计算出一个周期的时长；fundamentalFreq = 1.0 / pulsarPeriodTime
    float fundamentalFreq = 0.0;
    //ratio = pulse length/(pulse length+silence length)
    float pulsarDutyCycleRatio = 0.5f;
    float pulsarPeriodTime = 0.0f;
    float pulsarSilence = 0.0f;

    //mask之前train dutycyle由于多个pulsar period构成，如p s p s，counter表示下标，如0，1，,2
    int pulsarStageIndexInTrainDutyCycle = 0;
    //pulsar phase
    float pulsaretPhase = 0.0f;

    //===========================pulsaret envelope===========================
    juce::ADSR pulsarAdsr;
    juce::ADSR::Parameters pulsarAdsrParams;
    //是否触发release阶段，用来判断某个envelope阶段内只触发一次release阶段
    bool isTriggeredReleaseFlag;

    //===========================公用属性===========================
    std::shared_ptr<CommonVoiceSate> commonVoiceSate;

public:
    PulsarSynthVoice()
    {
    }

    //==============================重写方法==============================
    void startNote(int midiNoteNumber,
                   float velocity,
                   SynthesiserSound* sound,
                   int currentPitchWheelPosition
    ) override;

    void stopNote(float /*velocity*/, bool allowTailOff) override;
    void processSampleWithConvolution(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples,
                                      why::PlayModeEnum currentPlayModeEnum);

    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;
    void renderNextBlockDirectly(juce::AudioSampleBuffer& outputBuffer, juce::AudioPlayHead* audioPlayHead,
                                 int startSample,
                                 int numSamples, why::PlayModeEnum currentPlayModeEnum);

    void pitchWheelMoved(int) override;

    void controllerMoved(int, int) override;

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    //=============================老的===================================
    void setPulsarSilence(float dutyCylce);
    void changeToNewTrainAfterPulsarPeriodOrTrainEnd(bool bpmChangedFlag);
    void realChangeTrain();
    void initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);
    void resetTrainInitialSate();
    void resetTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);

    /**
     * 初始化所有步骤
     * @param sampleRate
     * @param sampleBuffer
     * @param audioPlayHead
     */
    void initSynth(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                   juce::AudioPlayHead* audioPlayHead);

    void generateStochasticMask();

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
    /**
     * 仅映射parameterChanged事件中的单个parameter，因为只有这个值保证最新
     * @param apvts
     * @param parameterID
     * @param newValue
     */
    void mappingOneParam(const juce::AudioProcessorValueTreeState& apvts, juce::String parameterID, float newValue);

    void parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID,
                          float newValue, bool& isGeneratedStochasticMask);

    void reloadPreset(juce::AudioProcessorValueTreeState& apvts);

    /**
     * 刷新adsr（应用在单个pulse上）
     * @param pulsarDutyCycleTime
     */
    void refreshPulsaretAdsr(float pulsarDutyCycleTime);
    void mask(bool& maskPassFlag, bool& existMask);
    void changeStage(int pulsarSamples, int intraSilenceSamples, int interTrainSilenceSamples,
                     int trainDutyCycleSamples);
    void calcNewPulsarFreq(float& pulsarModFreq, bool silenceToPulseFlag);
    float calSampleByState(bool passMaskFlag, bool existMask, float pulsarModFreq);
    void resetTrainRelatedSamples4Location();

    //刚启动为什么会卡一下：debug在开始时会卡一下，run没问题
    float processSample();

    float calcActualPulse(float pulsarModFreq);

    //将slider值映射到数组中的两个相邻波形之间
    float calcFormantLfoInterpolation(float pulsarModFreq, float amount);

    float calcAmpLfoInterpolation(float pulsarModFreq, float amount);

    float calcSample();

    [[nodiscard]] bool isCurrentCacluatedPulse() const
    {
        return currentSampleInPulse;
    }

    [[nodiscard]] std::atomic<float>*& getTrainDutyCycleLenParam()
    {
        return commonVoiceSate->trainDutyCycleLenParam;
    }

    void setTrainDutyCycleLenParam(std::atomic<float>* trainDutyCycleLenParam)
    {
        commonVoiceSate->trainDutyCycleLenParam = trainDutyCycleLenParam;
    }

    float getOutputGain();

    // void refreshBurstMask(juce::String burstMask);

    int refreshBpm(juce::AudioPlayHead* audioPlayHead);
    bool updateBpmDirectly(float bpm);

    // void rebuildTrainByBpm(juce::AudioPlayHead* audioPlayHead);

    [[nodiscard]] std::shared_ptr<CommonVoiceSate>& getCommonVoiceSate()
    {
        return commonVoiceSate;
    }

    void setCommonVoiceSate(std::shared_ptr<CommonVoiceSate>& commonVoiceSate)
    {
        this->commonVoiceSate = commonVoiceSate;
    }

    [[nodiscard]] bool& isSoundOffWhenSwitchPlayMode()
    {
        return soundOffWhenSwitchPlayMode;
    }

    void setSoundOffWhenSwitchPlayMode(bool soundOffWhenSwitchPlayMode)
    {
        this->soundOffWhenSwitchPlayMode = soundOffWhenSwitchPlayMode;
    }
};
