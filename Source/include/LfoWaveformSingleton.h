//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

class LfoWaveformSingleton
{
public:
    //get singleton instance
    static LfoWaveformSingleton& getInstance(double sampleRate, float frequency)
    {
        static LfoWaveformSingleton instance(sampleRate, frequency);
        return instance;
    }

    // 删除拷贝构造函数和赋值操作符，确保只能通过 getInstance 获取唯一实例
    LfoWaveformSingleton(const LfoWaveformSingleton&) = delete;
    LfoWaveformSingleton& operator=(const LfoWaveformSingleton&) = delete;

    //return each LFOs for external use
    std::vector<juce::dsp::Oscillator<float>*>& getAmpLfos() { return ampLfos; }
    std::vector<juce::dsp::Oscillator<float>*>& getFormantLfos() { return formantLfos; }

    /**
     * calcuate sample after applying am modulation, to implement the smooth movement between different waveforms, that
     * means when originalIndex stay between two indexs then it features two waveform sound
     *
     * @param originalIndex the smooth index
     * @param phase phase in waveform
     * @return
     */
    float calcSampleAfterAM(float originalIndex, float phase)
    {
        int size = getAmpLfos().size();
        float index = originalIndex * (size - 1);
        int lowerIndex = static_cast<int>(index);
        int upperIndex = std::min(lowerIndex + 1, size - 1);
        float factor = index - lowerIndex;
        float s = getAmpLfos()[lowerIndex]->processSample(phase) +
            (getAmpLfos()[upperIndex]->processSample(phase) -
                getAmpLfos()[lowerIndex]->processSample(phase)) *
            factor;
        s = juce::jlimit(-1.0f, 1.0f, s);
        return s;
    }

    float calcSampleAfterFM(float originalIndex, float phase)
    {
        int size = formantLfos.size();
        float index = originalIndex * (size - 1);
        int lowerIndex = static_cast<int>(index);
        int upperIndex = std::min(lowerIndex + 1, size - 1);
        float factor = index - lowerIndex;
        float s = getFormantLfos()[lowerIndex]->processSample(phase) +
            (formantLfos[upperIndex]->processSample(phase) -
                formantLfos[lowerIndex]->processSample(phase)) *
            factor;
        s = juce::jlimit(-1.0f, 1.0f, s);
        return s;
    }

private:
    // 私有构造函数，确保不能在外部创建实例
    LfoWaveformSingleton(double sampleRate, float freq)
    {
        this->sampleRate = sampleRate;
        initializeWaveforms(sampleRate, freq);
    }

    // 更新所有LFOs的频率
    void updateFrequencies()
    {
        for (auto* lfo : ampLfos)
            lfo->setFrequency(freq);

        for (auto* lfo : formantLfos)
            lfo->setFrequency(freq);
    }

    // 初始化波形
    void initializeWaveforms(double sampleRate, float fundamentalFreq)
    {
        // 波形生成逻辑
        auto makeTriangle = [](float x)
        {
            float normX = x / juce::MathConstants<float>::twoPi;
            return 2.0f * std::abs(2.0f * (normX - std::floor(normX + 0.5f))) - 1.0f;
        };

        auto makeSaw = [](float x)
        {
            return juce::jmap(x, 0.0f, juce::MathConstants<float>::twoPi, -1.0f, 1.0f);
        };

        auto makeSquare = [](float x)
        {
            return (x < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
        };

        auto makeSine = [](float x)
        {
            return std::sin(x);
        };

        auto makeComplexWave = [](float x)
        {
            return std::sin(x);
        };

        auto makeRoundedTriangle = [](float x)
        {
            return 2.0f * std::abs(std::sin(juce::MathConstants<float>::pi * x)) - 1.0f;
        };

        auto makeSoftSquare = [](float x)
        {
            return std::tanh(3.0f * std::sin(2.0f * juce::MathConstants<float>::pi * x));
        };

        auto makePwm = [](float x)
        {
            return (x < 0.25f) ? 1.0f : -1.0f;
        };

        auto makeSmoothRand = [](float x)
        {
            // 平滑过渡的随机波形，适合 LFO morphing（伪 Perlin 可替代）
            static float prev = 0.0f;
            static float next = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            float frac = x - std::floor(x);
            if (frac < 0.01f)
            {
                prev = next;
                next = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            }
            return juce::jmap(frac, 0.0f, 1.0f, prev, next); // 线性插值
        };

        auto makeNoise = [](float x)
        {
            return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        };
        // 初始化 Amp LFOs
        ampLfoSine.prepare({sampleRate, 512, 1});
        ampLfoSine.setFrequency(fundamentalFreq);
        ampLfoSine.initialise(makeSine);

        ampLfoComplexWave.prepare({sampleRate, 512, 1});
        ampLfoComplexWave.setFrequency(fundamentalFreq);
        ampLfoComplexWave.initialise(makeComplexWave);

        ampLfoRoundedTriangle.prepare({sampleRate, 512, 1});
        ampLfoRoundedTriangle.setFrequency(fundamentalFreq);
        ampLfoRoundedTriangle.initialise(makeRoundedTriangle);

        ampLfoTriangle.prepare({sampleRate, 512, 1});
        ampLfoTriangle.setFrequency(fundamentalFreq);
        ampLfoTriangle.initialise(makeTriangle);

        ampLfoSoftSquare.prepare({sampleRate, 512, 1});
        ampLfoSoftSquare.setFrequency(fundamentalFreq);
        ampLfoSoftSquare.initialise(makeSoftSquare);

        ampLfoPwm.prepare({sampleRate, 512, 1});
        ampLfoPwm.setFrequency(fundamentalFreq);
        ampLfoPwm.initialise(makePwm);

        ampLfoSquare.prepare({sampleRate, 512, 1});
        ampLfoSquare.setFrequency(fundamentalFreq);
        ampLfoSquare.initialise(makeSquare);

        ampLfoSaw.prepare({sampleRate, 512, 1});
        ampLfoSaw.setFrequency(fundamentalFreq);
        ampLfoSaw.initialise(makeSaw);

        ampLfoSmoothRand.prepare({sampleRate, 512, 1});
        ampLfoSmoothRand.setFrequency(fundamentalFreq);
        ampLfoSmoothRand.initialise(makeSmoothRand);

        ampLfoNoise.prepare({sampleRate, 512, 1});
        ampLfoNoise.setFrequency(fundamentalFreq);
        ampLfoNoise.initialise(makeNoise);

        ampLfos.push_back(&ampLfoSine);
        ampLfos.push_back(&ampLfoComplexWave);
        ampLfos.push_back(&ampLfoRoundedTriangle);
        ampLfos.push_back(&ampLfoTriangle);
        ampLfos.push_back(&ampLfoSaw);
        ampLfos.push_back(&ampLfoSoftSquare);
        ampLfos.push_back(&ampLfoPwm);
        ampLfos.push_back(&ampLfoSquare);
        ampLfos.push_back(&ampLfoSmoothRand);
        ampLfos.push_back(&ampLfoNoise);

        // 初始化 Formant Freq LFOs
        formantFreqSine.prepare({sampleRate, 512, 1});
        formantFreqSine.setFrequency(fundamentalFreq);
        formantFreqSine.initialise(makeSine);

        formantFreqComplexWave.prepare({sampleRate, 512, 1});
        formantFreqComplexWave.setFrequency(fundamentalFreq);
        formantFreqComplexWave.initialise(makeComplexWave);

        formantFreqRoundedTriangle.prepare({sampleRate, 512, 1});
        formantFreqRoundedTriangle.setFrequency(fundamentalFreq);
        formantFreqRoundedTriangle.initialise(makeRoundedTriangle);

        formantFreqTriangle.prepare({sampleRate, 512, 1});
        formantFreqTriangle.setFrequency(fundamentalFreq);
        formantFreqTriangle.initialise(makeTriangle);

        formantFreqSoftSquare.prepare({sampleRate, 512, 1});
        formantFreqSoftSquare.setFrequency(fundamentalFreq);
        formantFreqSoftSquare.initialise(makeSoftSquare);

        formantFreqPwm.prepare({sampleRate, 512, 1});
        formantFreqPwm.setFrequency(fundamentalFreq);
        formantFreqPwm.initialise(makePwm);

        formantFreqSquare.prepare({sampleRate, 512, 1});
        formantFreqSquare.setFrequency(fundamentalFreq);
        formantFreqSquare.initialise(makeSquare);

        formantFreqSaw.prepare({sampleRate, 512, 1});
        formantFreqSaw.setFrequency(fundamentalFreq);
        formantFreqSaw.initialise(makeSaw);

        formantFreqSmoothRand.prepare({sampleRate, 512, 1});
        formantFreqSmoothRand.setFrequency(fundamentalFreq);
        formantFreqSmoothRand.initialise(makeSmoothRand);

        formantFreqNoise.prepare({sampleRate, 512, 1});
        formantFreqNoise.setFrequency(fundamentalFreq);
        formantFreqNoise.initialise(makeNoise);

        formantLfos.push_back(&formantFreqSine);
        formantLfos.push_back(&formantFreqComplexWave);
        formantLfos.push_back(&formantFreqRoundedTriangle);
        formantLfos.push_back(&formantFreqTriangle);
        formantLfos.push_back(&formantFreqSaw);
        formantLfos.push_back(&formantFreqSoftSquare);
        formantLfos.push_back(&formantFreqPwm);
        formantLfos.push_back(&formantFreqSquare);
        formantLfos.push_back(&formantFreqSmoothRand);
        formantLfos.push_back(&formantFreqNoise);
    }

    // 数据成员
    float freq; // 当前的频率
    double sampleRate; // 当前的频率
    juce::dsp::Oscillator<float> ampLfoSine;
    juce::dsp::Oscillator<float> ampLfoComplexWave;
    juce::dsp::Oscillator<float> ampLfoSoftSquare;
    juce::dsp::Oscillator<float> ampLfoRoundedTriangle;
    juce::dsp::Oscillator<float> ampLfoPwm;
    juce::dsp::Oscillator<float> ampLfoTriangle;
    juce::dsp::Oscillator<float> ampLfoSquare;
    juce::dsp::Oscillator<float> ampLfoSaw;
    juce::dsp::Oscillator<float> ampLfoSmoothRand;
    juce::dsp::Oscillator<float> ampLfoNoise;

    juce::dsp::Oscillator<float> formantFreqSine;
    juce::dsp::Oscillator<float> formantFreqComplexWave;
    juce::dsp::Oscillator<float> formantFreqSoftSquare;
    juce::dsp::Oscillator<float> formantFreqRoundedTriangle;
    juce::dsp::Oscillator<float> formantFreqPwm;
    juce::dsp::Oscillator<float> formantFreqTriangle;
    juce::dsp::Oscillator<float> formantFreqSquare;
    juce::dsp::Oscillator<float> formantFreqSaw;
    juce::dsp::Oscillator<float> formantFreqSmoothRand;
    juce::dsp::Oscillator<float> formantFreqNoise;


    std::vector<juce::dsp::Oscillator<float>*> ampLfos;
    std::vector<juce::dsp::Oscillator<float>*> formantLfos;
};
