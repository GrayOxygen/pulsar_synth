//
// Created by Wang on 2025/4/25.
//

#ifndef SHAREDVOICESTATE_H
#define SHAREDVOICESTATE_H
class SharedVoiceSate{
 //====================================DAW PLAY*STOP control====================================
    bool wasPlayingLastFrame;
    //====================================train====================================
    //默认1/32 拍的时间
    double trainLenBlock;
    //默认1个bar
    double trainTime;

    //train周期长，秒
    float trainPeriodTime = 0.0f;
    //实际发送脉冲period的时长，秒
    float trainDutyCycleTime = 0.0f;
    //train之间的silence时长，秒
    float trainSilenceTime = 0.0f;

    //已走完的train数，非循环模式则播放一次就结束
    int trainCounter = 0;
    //tain结束后是否循环
    bool isLoop = true;

    //train duty cycle长度：多少个pulsar period
    float trainDutyCycleNum = 0.0f;
    //train silence长度：多少个pulsar period
    float trainSilenceNum = 0.0f;
    //train len：多少个trainLenBlock
    float trainLen = 0.0f;

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

    //===========================masking===========================
    why::MaskOptionEnum maskOption = why::MaskOptionEnum::Off;
    //burst
    //比如111010表示pulse pulse pulse silence pulse silence
    //假设一个train有1个pulsar period的train duty cycle构成，2个pulsar period长的train silence构成
    //那么01101则表示原来的train duty cycle为pulse silence变为silence pulse
    //如果train duty cycle有两个pulsar period，即pulse silence pulse silence将变成silence pulse pulse silence
    std::string burstMask = "";
    //euclid
    //表示几个pulsar duty cycle的长度
    std::string euclids;
    //stochastic
    std::string stochasticMaskStr = "";

    //===========================pulsaret envelope===========================
    juce::ADSR pulsarAdsr;
    juce::ADSR::Parameters pulsarAdsrParams;
    //是否触发release阶段，用来判断某个envelope阶段内只触发一次release阶段
    bool isTriggeredReleaseFlag;

    //===========================mapping parameter of plugin processor===========================
    //parameters to receive values from AudioProcessorValueTreeState，线程安全
    std::atomic<float>* outputGainParam;
    std::atomic<float>* playModeParam;
    std::atomic<float>* trainDutyCycleLenParam;
    std::atomic<float>* trainSilenceParam;
    //1 beat为单位，4表示1个bar
    std::atomic<float>* trainLenParam;

    std::atomic<float>* pulsarDutyCycleRatioParam;
    std::atomic<float>* pulsarWaveformParam;
    //一个pulse区域可以设置为多个pulse，长度以原本的pulse width为单位
    std::atomic<float>* pulsarDutyCycleClusterLenParam;

    std::atomic<float>* formantFreqLfoParam;
    std::atomic<float>* ampLfoParam;

    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;
    std::atomic<float>* maskOptionParam;

    //欧几里得节奏，也相当于一种mask
    std::atomic<float>* euclidStepsParam;
    std::atomic<float>* euclidHitsParam;

    //菜单选择
    std::atomic<float>* impulseSwitchParam;

    //===========================TODO 扫描采样，采样作为pulsaret===========================
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
    float readHead = 0;

    //===========================卷积===========================
    //初始voice时初始convolution，共享synth同一个ConvolutionResource
    //线程安全，因为只有一处写（由plugineditor触发在PulsarSynth类中修改，无写的并发），其余都是读
    std::shared_ptr<ConvolutionResource> convolutionResource;

    //===========================pulse buffer：用来批量处理convolution===========================
    juce::AudioBuffer<float> pulseBuffer;
}
#endif //SHAREDVOICESTATE_H
