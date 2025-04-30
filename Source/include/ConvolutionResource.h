//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "BinaryResourceSingleton.h"

#include "Commons.h"
/**
* All synth and voice share a ConvolutionResource to maintain impulse file data and do some convolution logic
* When initialized in prepareToPlay, if the sample rate, sample per block, and num channels change,
* this SynthEngine will reinitialize the ConvolutionResource otherwise keep the instance same.
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
     * According to the file id, read the impulse file under Resources(binary) and save it to this object (in memory),
     * and then directly load it as impulse response
     *
     * @param selectedId The id of the impulse file combobox, that is, the id of the binary file under Resources
     */
    void saveTemplateImpulse(int selectedId)
    {
        double fileSampleRate;
        std::unique_ptr<juce::AudioBuffer<float>> bf;
        juce::String resourceName = why::resourceIdToName[selectedId];

        if (resourceName.isNotEmpty() && BinaryResourceSingleton::getInstance().readFileFromResources(
            resourceName.toRawUTF8(), fileSampleRate, bf))
        {
            saveLastTemplateImpulseData(*bf, fileSampleRate, resourceName.toRawUTF8());
            loadTemplateImpulseFile();
        }
    }

    /**
     * Save the file data to this ConvolutionResource object (in memory)
     * @param file file impulse file, such as the impulse file selected from the file selection window
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
     * use saved memory block to load it as impulse response
     */
    void loadSampleImpulseFile()
    {
        if (getLastSampleImpulseMemoryBlock()->getSize() <= 0)
        {
            setShouldUseConvolution(false);
            return;
        }
        setShouldUseConvolution(true);

        //real load impulse file
        float irSize = getLastSampleImpulseMemoryBlock()->getSize();
        if (irSize > why::sampleRate)
        {
            irSize = 2046;
        }
        convolution->loadImpulseResponse(
            getLastSampleImpulseMemoryBlock()->getData(),
            getLastSampleImpulseMemoryBlock()->getSize(),
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            //TODO There is no limit. If a large file is passed in, it will consume a lot of performance. Originally,
            //1024*8 sample sizes were directly fixed, but later it can be changed to allow users to modify them manually
            0,
            juce::dsp::Convolution::Normalise::yes); // 0 = full IR
    }

    /**
     * use template buffer to load it as impulse response
     */
    void loadTemplateImpulseFile()
    {
        if (lastTemplateBufferSampleRate <= 0 || lastTemplateBuffer->getNumSamples() <= 0)
        {
            setShouldUseConvolution(false);
            return;
        }
        setShouldUseConvolution(true);

        //Avoid the data inside the pointer becoming empty after the move, as the data in the buffer has been moved
        std::unique_ptr<juce::AudioBuffer<float>> clonedBuffer = std::make_unique<juce::AudioBuffer<float>>(
            *lastTemplateBuffer);

        //The built-in impulse audio has all been processed and won't be too long
        convolution->loadImpulseResponse(
            std::move(*clonedBuffer),
            lastTemplateBufferSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            juce::dsp::Convolution::Normalise::yes);
    }

    /**
     * do convolution logic
     * @param pulseBuffer Data sources that require convolution
     * @return Return the sample after convolution
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
        //config convolution
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = samplesPerBlock;
        spec.numChannels = numChannels;
        convolution->prepare(spec);
    }

    //Used to store the last selected sample impulse file
    std::unique_ptr<juce::MemoryBlock> lastSampleImpulseMemoryBlock = std::make_unique<juce::MemoryBlock>();
    juce::MemoryBlock lastImpulseMemoryBlock;
    std::string sampleImpulseFilePath;

    //Used to store the template impulse file of the last selection
    std::unique_ptr<juce::AudioBuffer<float>> lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

    double lastTemplateBufferSampleRate = 0;

    //Whether the conditions for performing convolution operations are met
    //(to avoid invalid operations and affect performance) :
    //The impulse menu selects sample impulse but memory block is empty, which is false
    //When template impulse is selected in the impulse menu but there is no template buffer, it is false
    bool shouldUseConvolution = false;

    std::shared_ptr<juce::dsp::Convolution> convolution = std::make_shared<juce::dsp::Convolution>();
    juce::dsp::ProcessSpec spec;
};
