//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "Commons.h"
#include "CommonVoiceSate.h"

/**
 *  pulsar synth voice：pulsar核心逻辑在voice中实现
 */
class PulsarSynthVoice : public juce::SynthesiserVoice
{
private:
    //====================================Auto模式的属性====================================
    //上一次是否处于播放状态，用来判断哪一次属于第一次开始播放（而不是播放之后）
    bool wasPlayingLastFrame;
    //true：因为切换模式需要静音，false不需要，用来控制切换模式暂停声音，让用户再次播放（auto模式）或等midi note触发（midi模式）
    bool soundOffWhenSwitchPlayMode;

    //===========================midi模式下的属性===========================
    //是否允许播放，如midi note正在播放，没有note了就是停止
    bool playing = false;
    //应用于midi note的envelope，并不是针对于pulsar本身
    juce::ADSR envelope;
    //当前音符的力度
    float currentVelocity = 1.0;

    //====================================train====================================
    //默认1 beat的时间（秒）
    double trainLenBlock;
    //trainTime = trainLen * trainLenBlock;
    double trainTime;

    //========train duty cycle, train silence, train len都会直接导致train的变更，另外，bpm也是========
    //上一次的train duty cycle长度：多少个pulsar period
    int previousTrainDutyCycleNum = 0;
    //上一次的train silence长度：多少个pulsar period
    int previousTrainSilenceNum = 0;
    //上一次的train长度：多少个trainLenBlock
    int previousTrainLen = 0;

    //train period time = trainDutyCycleTime + trainSilenceTime，秒
    float trainPeriodTime = 0.0f;
    //train duty cycle: 实际发送脉冲period的时长，秒
    float trainDutyCycleTime = 0.0f;
    //train duty cycle结束后，进入到train silence，秒
    float trainSilenceTime = 0.0f;

    //已走完train的次数，用于判断非循环模式则播放一次就结束，暂时只能循环播放
    int trainCounter = 0;

    //=====控制DAW play stop控制train变更走向=====
    //是否需要进入新的train，每次train变更都不会直接rebuild train，等待最近的pulsar silence or train silence结束再进入，
    //如果silence都为0，则在最近的pulse结束后进入下个train
    bool changeTrainTrace = false;
    //true: 相同的train，false：train相关参数变了，则可以允许change train trace
    bool isTheSameTrainConfig;
    //new train duration length
    int newTrainDurationLen;
    //new train intervale silence length
    int newTrainIntervalSilenceLen;
    //new train len
    int newTrainLen;
    //pulsar samples = pulsar duty cycle time (若存在cluster，这里也是pulsar duty cycle，即cluster的总和) * sampleRate
    int pulsarDutyCycleSamples;
    //pulsar silence time * sample rate
    int pulsarIntraSilenceSamples;
    //train interval silence time * sample rate
    int interTrainSilenceSamples;
    //train duty cycle time * sample rate
    int trainDutyCycleSamples;
    bool isActive = false;

    //当前pulse状态（StateEnum）时长，对应所需的sample个数
    int currentStateDurationSampleNum = 0.0;
    //当前状态（StateEnum）已处理的sample个数，-999表示初始状态，train的开始
    int hasPassedSampleNumInsideTrain = -999.0f;
    //在单个train中最新位置（单位：samples），每次进入到新train则会重新初始化
    int trainPositionSamples = 0.0;

    //当前输出是否为pulse(准备废弃)：在经过mask最终输出后，原来的silence转为pulse，也算是pulse，原来的pulse被mask转为silence则不算pulse
    bool currentSampleInPulse;
    //当前原始的阶段标识（经过mask之前，因为经过masking后）：
    why::PulsarStateEnum currentState = why::PulsarStateEnum::IntraSilence;
    //=====控制DAW play stop控制train变更走向=====

    //===========================pulsaret===========================
    //fundamental frequency of pulsar emitter，可以计算出一个周期的时长；fundamentalFreq = 1.0 / pulsarPeriodTime
    //这个属性，实际上没怎么用到，因为本插件主要关注在train了
    float fundamentalFreq = 0.0;
    //ratio = pulse duty cycle length/(pulse duty cycle length + silence length)
    float pulsarDutyCycleRatio = 0.5f;
    //pulsar period = pulsar duty cycle time + pulsar silence time
    float pulsarPeriodTime = 0.0f;
    //pulsar silence
    float pulsarSilenceTime = 0.0f;

    //pulsar stage变更次数计数，用来判断取mask上哪个值来判断是否放行：
    //mask处理之前，train dutycycle由于多个pulsar period构成，如p s p s，则表示1 2 3 4...
    int pulsarStageIndexInTrainDutyCycle = 0;
    //pulsaret phase
    float pulsaretPhase = 0.0f;

    //===========================pulsaret envelope===========================
    //应用于原始pulsar duty cycle上的adsr，存在pulsar cluster则会应用在整个pulsar cluster上
    juce::ADSR pulsarAdsr;
    juce::ADSR::Parameters pulsarAdsrParams;
    //是否触发过release阶段，用来实现主动触发release，当到达当前stage结束前指定时长就触发，但依此判断来避免重复触发
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

    //=============================自定义方法===================================
    /**
     * calculate pulsar silence time
     * @param dutyCylceRatio pulsar duty cycle ratio =  pulsar duty cycle/pulsar period
     */
    void setPulsarSilence(float dutyCylceRatio);

    /**
     * 如果train相关参数表明，train已经发生改变，则等待下一个pulsar silence or train silence结束进入到新的train，
     * 如果silence都为0，则在最近的pulse结束后，进入新的train
     *
     * @param bpmChangedFlag is bpm changed or not
     */
    void changeToNewTrainAfterPulsarPeriodOrTrainEnd(bool bpmChangedFlag);

    /**
     * 直接更新最新train config到synth中，没有延迟
     */
    void realChangeTrainConfig();

    /**
     * 初始化train
     *
     * @param durationLen train duty cycle length
     * @param intervalSilenceLen train interval silence length
     * @param isLoop loop or not
     * @param trainLen train length
     */
    void initTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);

    /**
     * reset train related status to initial
     */
    void resetTrainInitialSate();

    /**
     * 根据指定参数重置train
     *
     * midi模式下，startNote会触发
     * auto and midi模式下，train参数变更，processSample会触发
     * midi模式下，renderNextBlockDirectly首次播放时会触发
     *
     * @param durationLen train duty cycle length
     * @param intervalSilenceLen train interval silence
     * @param isLoop  loop or not
     * @param trainLen  train length
     */
    void resetTrain(int durationLen, int intervalSilenceLen, float isLoop, int trainLen);

    /**
     * !!!!!!!!!初始化所有train, pulsar相关config，相当于是总的初始化入口!!!!!!!!!
     *
     * @param sampleRate sample rate
     * @param sampleBuffer sample buffer
     * @param audioPlayHead audio play head to get play position
     */
    void initSynthVoice(double sampleRate, std::unique_ptr<juce::AudioBuffer<float>>& sampleBuffer,
                        juce::AudioPlayHead* audioPlayHead);

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
     * @param apvts tree state
     * @param parameterID parameter id
     * @param newValue up-to-date value
     */
    void mappingOneParam(const juce::AudioProcessorValueTreeState& apvts, juce::String parameterID, float newValue);

    /**
     * 当AudioProcessorValueTreeState的parameterChanged监听被回调后，该方法也会被触发
     *
     * @param apvts tree state
     * @param parameterID parameter id
     * @param newValue up-to-date value
     * @param isGeneratedStochasticMask has generated this time? true yes, false no
     */
    void parameterChanged(juce::AudioProcessorValueTreeState& apvts, juce::String parameterID,
                          float newValue, bool& isGeneratedStochasticMask);

    /**
     * 将prest的数据更新到voice中
     * @param apvts tree state
     */
    void reloadPreset(juce::AudioProcessorValueTreeState& apvts);

    /**
     * 刷新adsr（应用在单个pulse上）
     * @param pulsarDutyCycleTime
     */
    void refreshPulsaretAdsr(float pulsarDutyCycleTime);

    /**
     * 当前sample是否通过了mask的信息
     *
     * @param maskPassFlag 结果将写更新到这个参数：是否通过了mask
     * @param existMask 结果将写更新到这个参数：是否存在mask
     */
    void mask(bool& maskPassFlag, bool& existMask);

    /**
     * enter to the next stage
     *
     * @param pulsarDutyCycleSamples current pulsar duty cycle samples
     * @param intraSilenceSamples  current pulsar silence samples
     * @param interTrainSilenceSamples current train interval silence samples
     * @param trainDutyCycleSamples  current train duty cycle samples
     */
    void changeStage(int pulsarDutyCycleSamples, int intraSilenceSamples, int interTrainSilenceSamples,
                     int trainDutyCycleSamples);

    /**
     * calculate new modulated pulsar frequency
     *
     * 如果silenceToPulseFlag为false，则pulsar duty cycle作为频率计算依据
     * 如果silenceToPulseFlag为true，则pulsar silence长度作为频率计算依据
     *
     * @param pulsarModFreq 结果会修改到这个参数中
     * @param silenceToPulseFlag 是否曾经是silence现在是pulse
     */
    void calcNewPulsarFreq(float& pulsarModFreq, bool silenceToPulseFlag);

    /**
     * 根据state来分别计算最终的sample
     *
     * @param passMaskFlag 是否通过了mask
     * @param existMask 是否存在mask
     * @param pulsarModFreq modulate frequency
     * @return 
     */
    float calSampleByState(bool passMaskFlag, bool existMask, float pulsarModFreq);

    /**
     * reset train时，重新计算相关的samples，用来定位位置信息
     */
    void resetTrainRelatedSamples4Location();

    /**
     * auto,midi mode下都会执行该方法计算sample
     * @return calculated sample
     */
    float processSample();

    /**
     * calculate sample
     *
     * @param pulsarModFreq the newest frequency for sample calculation
     * @return sample
     */
    float calcActualPulse(float pulsarModFreq);

    /**
     * 在waveform table中光滑地过渡不同的波形，从而应用不同波形特点的FM modulation
     *
     * @param pulsarModFreq new pulsar modulation frequency
     * @param amount decide how much modulation, 0.0f - 1.0f
     * @return
     */
    float calcFormantLfoInterpolation(float pulsarModFreq, float amount);

    /**
     * 在waveform table中光滑地过渡不同的波形，从而应用不同波形特点的AM modulation
     *
     * @param pulsarModFreq new pulsar modulation frequency
     * @param amount decide how much modulation, 0.0f - 1.0f
     * @return
     */
    float calcAmpLfoInterpolation(float pulsarModFreq, float amount);

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

    /**
     * get output gain by converting db value
     * @return output gain
     */
    float getOutputGain();

    /**
     * init bpm use DAW BPM
     * @param audioPlayHead audio play head
     * @return 1: changed a different bpm; 0 :no need to update
     */
    int initBpmFromDaw(juce::AudioPlayHead* audioPlayHead);

    /**
     * Just update bpm
     * @param bpm new bpm
     * @return true: changed a different bpm false:no need to update
     */
    bool updateBpmDirectly(float bpm);

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