//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

/**
 * The pulsaret waveform singleton maintains only one instance globally
 */
class PulsaretWaveformSingleton
{
public:
    static PulsaretWaveformSingleton& getInstance()
    {
        static PulsaretWaveformSingleton instance;
        return instance;
    }

    //Remove the copy constructor and assignment operator to ensure that only a unique instance can be obtained
    PulsaretWaveformSingleton(const PulsaretWaveformSingleton&) = delete;
    PulsaretWaveformSingleton& operator=(const PulsaretWaveformSingleton&) = delete;

    /**
     * calculate smooth modulation between the waveforms table
     * @param originalSampleIndex float index:0.0f-1.0f
     * @param phase phase
     * @return get the smooth modulation
     */
    float calcSample(float originalSampleIndex, float phase)
    {
        // 将slider值映射到waveformLUTs数组的两个相邻波形之间
        int numWaveforms = waveformLUTs.size();
        float index = originalSampleIndex * (numWaveforms - 1);

        int lowerIndex = static_cast<int>(index); // 选择下一个波形的索引
        int upperIndex = std::min(lowerIndex + 1, numWaveforms - 1); // 选择上一个波形的索引，确保不越界

        // 计算插值因子
        float interpolationFactor = index - lowerIndex;

        // 获取对应位置的两个波形
        juce::dsp::LookupTableTransform<float>& lowerWaveform = *waveformLUTs[lowerIndex];
        juce::dsp::LookupTableTransform<float>& upperWaveform = *waveformLUTs[upperIndex];

        float s = lowerWaveform.processSample(phase) + (upperWaveform.processSample(phase) - lowerWaveform.
            processSample(phase)) * interpolationFactor;
        s = juce::jlimit(-1.0f, 1.0f, s);
        return s;
    }

private:
    //Private constructor to ensure that instances cannot be created externally
    PulsaretWaveformSingleton()
    {
        initializeWaveforms();
    }

    //The reason for using LookupTableTransform instead of juce::dsp::Oscillator because I need control the phase
    juce::dsp::LookupTableTransform<float> sineLUT;
    juce::dsp::LookupTableTransform<float> roundedTriangleLUT;
    juce::dsp::LookupTableTransform<float> triangleLUT;
    juce::dsp::LookupTableTransform<float> softSquareLUT;
    juce::dsp::LookupTableTransform<float> pwmLUT;
    juce::dsp::LookupTableTransform<float> sawLUT;
    juce::dsp::LookupTableTransform<float> squareLUT;
    juce::dsp::LookupTableTransform<float> smoothRandLUT;
    juce::dsp::LookupTableTransform<float> steppedRandLUT;
    juce::dsp::LookupTableTransform<float> noiseLUT;
    juce::dsp::LookupTableTransform<float> complexWaveLUT;

    //pulsaret waveform
    std::vector<juce::dsp::LookupTableTransform<float>*> waveformLUTs;

    /**
     * init waveform
     * @param lut lookup对象
     * @param waveformFunc 应用的波形函数
     */
    void initializeWaveform(juce::dsp::LookupTableTransform<float>& lut, std::function<float(float)> waveformFunc)
    {
        lut.initialise(waveformFunc, -1.0f, 1.0f, 4096);
    }

    /**
      由柔到硬的 LFO 波形序列（适用于 noise LUT 或 LFO morphing）
      1. Sine
         - 极度平滑，天然的周期性波形。
         - 公式: y = sin(2 * π * x)
      2. Rounded Triangle
         - 比标准三角波更圆润，过渡更加自然。
         - 公式: y = 2 * abs(sin(π * x)) - 1
      3. Triangle
         - 线性上升下降的波形，有清晰的边缘但无断裂。
         - 公式: y = 4 * abs(x - 0.5) - 1
      4. Soft Square
         - 类似方波但边缘圆滑，通过 tanh 压缩 sin 实现。
         - 公式: y = tanh(3 * sin(2 * π * x))
      5. Pulse (25% Duty) PWM
         - 占空比为 25% 的方波，节奏感强。
         - 公式: y = (x < 0.25) ? 1.0 : -1.0
      6. Sawtooth
         - 从 -1 到 1 线性上升的斜坡波，含丰富谐波。
         - 公式: y = 2 * x - 1
      7. Square
         - 最简方波，完全不连续，从 +1 跳到 -1。
         - 公式: y = (x < 0.5) ? 1.0 : -1.0
      8. Smooth Sample & Hold
         - 随机值平滑过渡（可用 Perlin 或插值噪声）。
         - 伪代码: y = lerp(rand[n-1], rand[n], frac)
      9. Stepped Random (Sample & Hold)
         - 每固定步长输出一个新的随机值，最为“粗糙”。
         - 伪代码: y = rand_table[floor(x * N)]
    */
    void initializeWaveforms()
    {
        // 初始化所有的 Lookup Table（LUT）
        initializeWaveform(sineLUT, [](float x)
        {
            // 极度平滑，天然周期性
            return std::sin(2.0f * juce::MathConstants<float>::pi * x);
        });

        initializeWaveform(roundedTriangleLUT, [](float x)
        {
            // 比标准三角波更圆润，过渡更自然
            return 2.0f * std::abs(std::sin(juce::MathConstants<float>::pi * x)) - 1.0f;
        });

        initializeWaveform(complexWaveLUT, [](float x)
        {
            float wave = std::sin(2.0f * juce::MathConstants<float>::pi * x);
            wave += 0.5f * std::sin(4.0f * juce::MathConstants<float>::pi * x); // 和谐波叠加
            wave += 0.25f * std::sin(8.0f * juce::MathConstants<float>::pi * x);
            return wave;
        });

        initializeWaveform(triangleLUT, [](float x)
        {
            // 清晰边缘但连续不断裂的线性波形
            return 2.0f * std::abs(2.0f * (x - std::floor(x + 0.5f))) - 1.0f;
        });

        initializeWaveform(softSquareLUT, [](float x)
        {
            // 使用 tanh 压缩 sin，模拟方波但具有柔和边缘
            return std::tanh(3.0f * std::sin(2.0f * juce::MathConstants<float>::pi * x));
        });

        initializeWaveform(pwmLUT, [](float x)
        {
            // 脉冲波，占空比 25%，节奏感更强
            return (x < 0.25f) ? 1.0f : -1.0f;
        });

        initializeWaveform(sawLUT, [](float x)
        {
            // 从 -1 到 1 的斜坡波，富含谐波，强烈感知
            return 2.0f * (x - std::floor(x + 0.5f));
        });

        initializeWaveform(squareLUT, [](float x)
        {
            // 最硬的基本波形，+1 到 -1 的极端跳变
            return (x < 0.5f) ? -1.0f : 1.0f;
        });

        initializeWaveform(smoothRandLUT, [](float x)
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
        });

        initializeWaveform(noiseLUT, [](float /*x*/)
        {
            // 随机跳变的采样保持波形，最“粗糙”
            return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        });
        initializeWaveform(steppedRandLUT, [](float x)
        {
            // 使用 Sample & Hold 生成 "Stepped Random" 波形，每步一个新的随机值
            // x * N 是为了根据步长来选择随机值
            constexpr int N = 128; // N 是步数，决定了波形的分辨率
            int index = std::floor(x * N);
            return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; // 随机值范围 -1 到 1
        });

        // The push_back order corresponds to the values of the index from 0.0f to 1.0f. Generally speaking,
        // this reflects the characteristic from smooth to non-smooth, and the order is consistent with
        // the waveform table order of lfo
        waveformLUTs.push_back(&sineLUT);
        waveformLUTs.push_back(&complexWaveLUT);
        waveformLUTs.push_back(&roundedTriangleLUT);
        waveformLUTs.push_back(&triangleLUT);
        waveformLUTs.push_back(&sawLUT);
        waveformLUTs.push_back(&softSquareLUT);
        waveformLUTs.push_back(&pwmLUT);
        waveformLUTs.push_back(&squareLUT);
        waveformLUTs.push_back(&smoothRandLUT);
        waveformLUTs.push_back(&steppedRandLUT);
        // waveformLUTs.push_back(&noiseLUT);
    }
};
