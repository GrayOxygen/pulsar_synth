//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "Commons.h"

//TODO 考虑清空数据
class ConvolutionResource
{
public:
    ConvolutionResource(double sampleRate, int samplesPerBlock, int numChannels)
    {
        prepare(sampleRate, samplesPerBlock, numChannels);
    }

    //get singleton instance
    // static ConvolutionResource& getInstance(double sampleRate, int samplesPerBlock, int numChannels)
    // {
    //     static ConvolutionResource instance(sampleRate, samplesPerBlock, numChannels);
    //     return instance;
    // }

    // 删除拷贝构造函数和赋值操作符，确保只能通过 getInstance 获取唯一实例
    // ConvolutionResource(const ConvolutionResource&) = delete;
    // ConvolutionResource& operator=(const ConvolutionResource&) = delete;

    [[nodiscard]] std::shared_ptr<juce::File>& getCurrentImpulseFile()
    {
        return currentImpulseFile;
    }

    void setCurrentImpulseFile(const std::shared_ptr<juce::File>& currentImpulseFile)
    {
        this->currentImpulseFile = currentImpulseFile;
    }

    [[nodiscard]] juce::MemoryBlock& getLastImpulseMemoryBlock1()
    {
        return lastImpulseMemoryBlock;
    }

    [[nodiscard]] bool& isSetupFlag()
    {
        return setupFlag;
    }

    void setSetupFlag(bool setupFlag)
    {
        this->setupFlag = setupFlag;
    }

    [[nodiscard]] std::shared_ptr<juce::dsp::Convolution>& getConvolution()
    {
        return convolution;
    }

    void setConvolution(const std::shared_ptr<juce::dsp::Convolution>& convolution)
    {
        this->convolution = convolution;
    }

    void saveLastTemplateImpulseData(const juce::AudioBuffer<float>& lastTemplateBuffer,
                                     double lastTemplateDataSize, std::string fileName)
    {
        this->lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(lastTemplateBuffer);
        this->lastTemplateDataSize = lastTemplateDataSize;
        this->sampleImpulseFilePath = fileName;
    }

    void saveLastSampleFileAsBlock(const juce::File& file)
    {
        if (file.getSize() <= 0)
        {
            return;
        }
        // std::unique_ptr<juce::File> filePtr = std::make_unique<juce::File>(file);
        // processorRef.set_convolution_file(filePtr);
        juce::MemoryBlock memBlock;
        file.loadFileAsData(memBlock);
        std::unique_ptr<juce::MemoryBlock> memBlockPtr = std::make_unique<juce::MemoryBlock>(memBlock);
        setLastSampleImpulseMemoryBlock(memBlockPtr);
        this->sampleImpulseFilePath = file.getFullPathName().toStdString();
    }

    void loadSampleImpulseFile()
    {
        if (getLastSampleImpulseMemoryBlock()->getSize() <= 0)
        {
            //当前还没记载采样，也要清空template播放的卷积
            setShouldUseConvolution(false);
            return;
        }
        setShouldUseConvolution(true);

        //更新impulse file
        convolution->loadImpulseResponse(
            getLastSampleImpulseMemoryBlock()->getData(),
            getLastSampleImpulseMemoryBlock()->getSize(),
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            2048, //TODO 第一个FFT SIZE , 分区块做FFT,越大越耗时，越小越快，但声音分辨率越低
            juce::dsp::Convolution::Normalise::yes); // 0 = full IR
    }

    //保存最后的
    void loadTemplateImpulseFile()
    {
        if (lastTemplateDataSize <= 0)
        {
            //当前不使用卷积
            setShouldUseConvolution(false);
            return;
        }
        setShouldUseConvolution(true);

        //避免move后指针内部的数据变空，因为buffer的数据被move了
        std::unique_ptr<juce::AudioBuffer<float>> clonedBuffer = std::make_unique<juce::AudioBuffer<float>>(
            *lastTemplateBuffer);

        //自带的impulse audio都处理过了，不会太长
        convolution->loadImpulseResponse(
            std::move(*clonedBuffer),
            lastTemplateDataSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            juce::dsp::Convolution::Normalise::yes);

        //TODO 修改FFT SIZE
        // int numChannels = clonedBuffer->getNumChannels();
        // int numSamples = clonedBuffer->getNumSamples();
        // size_t sourceDataSize = numChannels * numSamples * sizeof(float);
        //
        // // 创建一个临时缓冲区来存储所有通道的数据
        // std::vector<float> combinedData(numChannels * numSamples);
        //
        // // 合并所有通道的数据
        // for (int channel = 0; channel < numChannels; ++channel)
        // {
        //     const float* channelData = clonedBuffer->getReadPointer(channel);
        //     std::copy(channelData, channelData + numSamples, combinedData.data() + (channel * numSamples));
        // }
        // // 将数据传递给 Convolution
        // convolution->loadImpulseResponse(
        //     combinedData.data(),
        //     sourceDataSize,
        //     (numChannels == 2) ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
        //     juce::dsp::Convolution::Trim::yes,
        //     why::sampleRate,
        //     juce::dsp::Convolution::Normalise::yes
        // );
    }

    bool processSample(juce::AudioBuffer<float>& pulseBuffer)
    {
        if (!shouldUseConvolution)
        {
            return false;
        }
        float postGain = 1.0;
        if (convolution->getCurrentIRSize() > 0 && shouldUseConvolution)
        {
            juce::dsp::AudioBlock<float> block(pulseBuffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            convolution->process(context);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::unique_ptr<juce::MemoryBlock>& getLastSampleImpulseMemoryBlock()
    {
        return lastSampleImpulseMemoryBlock;
    }

    void setLastSampleImpulseMemoryBlock(std::unique_ptr<juce::MemoryBlock>& lastSampleImpulseMemoryBlock)
    {
        this->lastSampleImpulseMemoryBlock = std::move(lastSampleImpulseMemoryBlock);
    }

    [[nodiscard]] std::unique_ptr<juce::AudioBuffer<float>>& getLastTemplateBuffer()
    {
        return lastTemplateBuffer;
    }

    [[nodiscard]] double& getLastTemplateDataSize()
    {
        return lastTemplateDataSize;
    }

    [[nodiscard]] bool& isShouldUseConvolution()
    {
        return shouldUseConvolution;
    }

    void setShouldUseConvolution(bool shouldUseConvolution)
    {
        this->shouldUseConvolution = shouldUseConvolution;
    }

    [[nodiscard]] juce::MemoryBlock& getLastImpulseMemoryBlock()
    {
        return lastImpulseMemoryBlock;
    }

    void setLastImpulseMemoryBlock(const juce::MemoryBlock& lastImpulseMemoryBlock)
    {
        this->lastImpulseMemoryBlock = lastImpulseMemoryBlock;
    }

private:
    void prepare(double sampleRate, int samplesPerBlock, int numChannels)
    {
        //配置convolution
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = samplesPerBlock; // 或者直接设为 2048
        spec.numChannels = numChannels;
        convolution->prepare(spec);
    }

    //最后一次选择的sample impulse block
    std::unique_ptr<juce::MemoryBlock> lastSampleImpulseMemoryBlock = std::make_unique<juce::MemoryBlock>();
    std::string sampleImpulseFilePath;

    //最后一次选择的template impulse buffer
    std::unique_ptr<juce::AudioBuffer<float>> lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
    std::string templateImpulseFileName;

    double lastTemplateDataSize = 0;
    bool shouldUseConvolution = false;

    std::shared_ptr<juce::dsp::Convolution> convolution = std::make_shared<juce::dsp::Convolution>();
    juce::dsp::ProcessSpec spec;
    std::shared_ptr<juce::File> currentImpulseFile;

    juce::MemoryBlock lastImpulseMemoryBlock;
    bool setupFlag = false;
};
