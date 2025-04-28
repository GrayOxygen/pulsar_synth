//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include <ResourceSingleton.h>

#include "Commons.h"
/**
 * 所有synth和voice都共享一个ConvolutionResource，维护impulse file data，实现convolution相关的逻辑
 *
 * 在prepareToPlay中初始化，若sample rate, sample per block, num channels变更，则该SynthEngine会重新初始化ConvolutionResource
 */
class ConvolutionResource
{
public:
    ConvolutionResource(double sampleRate, int samplesPerBlock, int numChannels)
    {
        prepare(sampleRate, samplesPerBlock, numChannels);
    }

    [[nodiscard]] juce::MemoryBlock& getLastImpulseMemoryBlock1()
    {
        return lastImpulseMemoryBlock;
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
                                     double lastTemplateBufferSampleRate, std::string fileName)
    {
        this->lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(lastTemplateBuffer);
        this->lastTemplateBufferSampleRate = lastTemplateBufferSampleRate;
        this->sampleImpulseFilePath = fileName;
    }

    /**
     * 根据文件id，读取Resources下impulse file并保存到该对象中（内存中），再直接load it as impulse response
     * @param selectedId impulse file combobox的id，也即Resources下binary file的id
     */
    void saveTemplateImpulse(juce::String selectedId)
    {
        const char* resourceName = (BinaryResourceSingleton::getInstance().getBinaryIdFileNameMap()[selectedId]).
            toRawUTF8();
        double fileSampleRate;
        std::unique_ptr<juce::AudioBuffer<float>> bf;

        if (why::readFileFromResources(resourceName, fileSampleRate, bf))
        {
            saveLastTemplateImpulseData(*bf, fileSampleRate, std::string(resourceName));
            loadTemplateImpulseFile();
        }
    }

    /**
     * 将file数据保存到该ConvolutionResource对象中（内存中）
     * @param file impulse file, 如从文件选择窗口中选择的impulse文件
     */
    void saveLastSampleFileAsBlock(const juce::File& file)
    {
        if (file.getSize() <= 0)
        {
            return;
        }
        juce::MemoryBlock memBlock;
        file.loadFileAsData(memBlock);
        std::unique_ptr<juce::MemoryBlock> memBlockPtr = std::make_unique<juce::MemoryBlock>(memBlock);
        setLastSampleImpulseMemoryBlock(memBlockPtr);
        this->sampleImpulseFilePath = file.getFullPathName().toStdString();
    }

    /**
     * 根据保存的memory block，load it as impulse response
     */
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
            0, //TODO 不设限制，如果传入很大的文件将会很耗费性能，原本直接写死1024*8个sample size，后面可改为让用户手动修改
            juce::dsp::Convolution::Normalise::yes); // 0 = full IR
    }

    /**
     * 根据template buffer, load it as impulse response
     */
    void loadTemplateImpulseFile()
    {
        if (lastTemplateBufferSampleRate <= 0 || lastTemplateBuffer->getNumSamples() <= 0)
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
            lastTemplateBufferSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            juce::dsp::Convolution::Normalise::yes);
    }

    /**
     * 做卷积运算
     * @param pulseBuffer 需要做卷积的数据源
     * @return 返回卷积后的sample
     */
    bool processSample(juce::AudioBuffer<float>& pulseBuffer)
    {
        if (!shouldUseConvolution)
        {
            return false;
        }
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
        return lastTemplateBufferSampleRate;
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

    //用于存储最后一次选择的sample impulse file
    std::unique_ptr<juce::MemoryBlock> lastSampleImpulseMemoryBlock = std::make_unique<juce::MemoryBlock>();
    juce::MemoryBlock lastImpulseMemoryBlock;
    std::string sampleImpulseFilePath;

    //用于存储最后一次选择的template impulse file
    std::unique_ptr<juce::AudioBuffer<float>> lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

    double lastTemplateBufferSampleRate = 0;

    //是否具备做卷积运算的条件（避免无效运算，影响性能）：
    //impulse菜单选中sample impulse，却memory block，则为false
    //impulse菜单选中template impulse，却无template buffer时，则为false
    bool shouldUseConvolution = false;

    std::shared_ptr<juce::dsp::Convolution> convolution = std::make_shared<juce::dsp::Convolution>();
    juce::dsp::ProcessSpec spec;
};
