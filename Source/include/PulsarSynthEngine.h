//
// Created by Wang on 2025/4/24.
//
#ifndef PULSARSYNTHENGINE_H
#define PULSARSYNTHENGINE_H
#include "Commons.h"
#include "PulsarSynth.h"
#include "PulsarSynthSound.h"

/**
 * pulsar synth engine to main multiple synth instances
 * auto and midi modes correspond to two different synths
 */
class PulsarSynthEngine
{
public:
    PulsarSynthEngine()
    {
    }

    /**
     * Obtain the synth currently in use
     * @return return auto synth by default
     */
    std::shared_ptr<PulsarSynth>& getCurrentPulsarSynth()
    {
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto)
        {
            return pulsarSynthForAuto;
        }
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Midi)
        {
            return pulsarSynthForMidi;
        }
        return pulsarSynthForAuto;
    }

    [[nodiscard]] why::PlayModeEnum& getCurrentPlayModeEnum()
    {
        return currentPlayModeEnum;
    }

    /**
     * Set the current playback mode and pause all sounds each time you set it
     *
     * @param apvts value tree
     * @param index play mode enum value
     */
    void setCurrentPlayModeEnum(int index)
    {
        stopTheWorld();
        if (index == static_cast<int>(why::PlayModeEnum::Auto))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::Auto;
        }
        if (index == static_cast<int>(why::PlayModeEnum::Midi))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::Midi;
        }
        if (index == static_cast<int>(why::PlayModeEnum::NotSelected))
        {
            this->currentPlayModeEnum = why::PlayModeEnum::NotSelected;
        }
    }

    /**
     * Stop all sounds: The midi mode will be automatically triggered as the next note plays,
     * while the auto mode requires the user to click play again
     */
    void stopTheWorld()
    {
        for (int channel = 1; channel <= 16; ++channel)
        {
            pulsarSynthForMidi->allNotesOff(channel, false);
        }
        pulsarSynthForAuto->triggerSoundOffWhenSwitchPlayMode();
    }

    /**
     * In the currently used synth, execute the specified method
     * @param func the target func
     */
    void executeCurSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        if (currentPlayModeEnum == why::PlayModeEnum::Auto)
        {
            func(pulsarSynthForAuto);
        }
        if (currentPlayModeEnum == why::PlayModeEnum::Midi)
        {
            func(pulsarSynthForMidi);
        }
    }

    /**
     * refresh the synth according to the preset, load files, etc
     * @param apvts tree state
     */
    void reloadSynthPreset(juce::AudioProcessorValueTreeState& apvts)
    {
        //refresh and load impulse file,
        int impulseSwitchIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseSwitch));

        //show the lastest template impulse option，and save convolution resource
        int index = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::impulseTemplateFile));

        if (impulseSwitchIndex == static_cast<int>(why::ImpulseSwitchEnum::Template))
        {
            getConvolutionResource()->saveTemplateImpulse(juce::String(index + 1));
        }

        //Display the latest sample impulse option and (must) directly load the file into the convolution resource
        //(because switch the impulse menu option to no longer save the file)
        String sampleImpulsePath = apvts.state.getProperty(why::PropertyID::sampleImpulsePath).toString();
        if (sampleImpulsePath.isNotEmpty())
        {
            juce::File file(sampleImpulsePath);
            if (file.existsAsFile())
            {
                // 文件存在，则保存资源文件到convolution resource中（全局的）
                getConvolutionResource()->saveLastSampleFileAsBlock(file);
                if (impulseSwitchIndex == static_cast<int>(why::ImpulseSwitchEnum::Sample))
                {
                    //加载impulse response
                    getConvolutionResource()->loadSampleImpulseFile();
                }
            }
        }

        //update current play mode
        int currentPlayModeIndex = static_cast<int>(*apvts.getRawParameterValue(why::ParameterID::playMode));
        setCurrentPlayModeEnum(currentPlayModeIndex);

        //update to the latest state of synth: like mapping the values of all parameters and properties to synth, etc
        executeCurSynthCallback([&](std::shared_ptr<PulsarSynth>& synth)
        {
            synth->reloadPreset(apvts);
        });
    }

    /**
     * Execute the specified method of all synth
     *
     * @param func The methods that need to be executed
     */
    void executeAllSynthCallback(const std::function<void(std::shared_ptr<PulsarSynth>&)>& func)
    {
        for (std::shared_ptr<PulsarSynth>& synth : pulsarSynths)
        {
            func(synth);
        }
    }

    /**
     * init convolution
     *
     * Note: When opening the DAW, prepareToPlay may be triggered multiple times.
     * If there is an initialization operation, idempotence must be guaranteed
     *
     * @param sampleRate sample rate
     * @param samplesPerBlock samples per block
     * @param numChannels nums of channel
     */
    void initConvolution(double sampleRate, int samplesPerBlock, int numChannels)
    {
        //防止prepareToPlay多次触发更新，保证幂等性
        if (!convolutionResource)
        {
            this->convolutionResource = std::make_shared<ConvolutionResource>(sampleRate, samplesPerBlock, numChannels);
        }

        for (std::shared_ptr<PulsarSynth> tempSynth : pulsarSynths)
        {
            for (int i = 0; i < tempSynth->getNumVoices(); ++i)
            {
                juce::SynthesiserVoice* voice = tempSynth->getVoice(i);
                PulsarSynthVoice* pulsarVoice = dynamic_cast<PulsarSynthVoice*>(voice);
                pulsarVoice->getCommonVoiceSate()->convolutionResource = convolutionResource;
            }
        }
    }

    /**
     * init synth, sound, voice
     * @param apvts value tree state
     */
    void init(juce::AudioProcessorValueTreeState& apvts)
    {
        //Prevent prepareToPlay from triggering updates multiple times to achieve idempotence
        if (!commonVoiceSate)
        {
            this->commonVoiceSate = std::make_shared<CommonVoiceSate>();
        }

        pulsarSynthForMidi->addSound(new PulsarSynthSound());
        //only support 4 voices
        for (int i = 0; i < 4; i++)
        {
            pulsarSynthForMidi->addVoice(new PulsarSynthVoice());

            PulsarSynthVoice* v = dynamic_cast<PulsarSynthVoice*>(pulsarSynthForMidi->getVoice(i));
            v->setCommonVoiceSate(commonVoiceSate);
            v->connectParameters(apvts);
        }

        pulsarSynthForAuto->addSound(new PulsarSynthSound());
        pulsarSynthForAuto->addVoice(new PulsarSynthVoice());

        PulsarSynthVoice* v = dynamic_cast<PulsarSynthVoice*>(pulsarSynthForAuto->getVoice(0));
        v->setCommonVoiceSate(commonVoiceSate);
        v->connectParameters(apvts);

        pulsarSynths.push_back(pulsarSynthForMidi);
        pulsarSynths.push_back(pulsarSynthForAuto);
    }

    /**
     * init convolution and build train
     * @param sampleRate sample rate
     * @param samplesPerBlock samples per block
     * @param numChannels num channels
     * @param playHead audio play head to get play position info
     */
    void buildTrain(double sampleRate, int samplesPerBlock, int numChannels, juce::AudioPlayHead* playHead)
    {
        pulsarSynthForMidi->setCurrentPlaybackSampleRate(sampleRate);
        pulsarSynthForAuto->setCurrentPlaybackSampleRate(sampleRate);

        this->initConvolution(sampleRate, samplesPerBlock, numChannels);
        //TODO 读取采样，用于使用采样做为pulsaret waveform
        // audioFormatManager->registerBasicFormats();
        // 创建一个 JUCE String 对象，包含路径
        // juce::String path = "/Users/blueear/Documents/Samples/test dd/rim.wav";
        //
        // // 对路径进行 URL 转义
        // juce::String escapedPath = juce::URL::addEscapeChars(path, false, false);
        // // 使用转码后的路径
        // juce::File file(path);
        //
        // audioFormatReader.reset(audioFormatManager->createReaderFor(file));
        // sampleBuffer->setSize(audioFormatReader->numChannels, audioFormatReader->lengthInSamples);
        // audioFormatReader->read(sampleBuffer.get(), 0, audioFormatReader->lengthInSamples, 0, true, true);

        pulsarSynthForMidi->initTrain(sampleRate, sampleBuffer, playHead);
        pulsarSynthForAuto->initTrain(sampleRate, sampleBuffer, playHead);
    }

    /**
     * the entry of processing sample
     *
     * @param buffer audio buffer
     * @param midiMessages midi message
     * @param playHead audio play head
     */
    void processSample(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, juce::AudioPlayHead* playHead)
    {
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Auto)
        {
            pulsarSynthForAuto->renderNextBlockDirectly(buffer, playHead, midiMessages, 0, buffer.getNumSamples(),
                                                        getCurrentPlayModeEnum());
        }
        if (getCurrentPlayModeEnum() == why::PlayModeEnum::Midi)
        {
            pulsarSynthForMidi->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
        }
    }

    /**
     * get the common shared convolution resource
     * @return convolution resource
     */
    [[nodiscard]] std::shared_ptr<ConvolutionResource>& getConvolutionResource()
    {
        return convolutionResource;
    }

private:
    //The two playback modes correspond to two synths, but for the user, just one
    std::shared_ptr<PulsarSynth> pulsarSynthForMidi = std::make_unique<PulsarSynth>(why::PlayModeEnum::Midi);
    std::shared_ptr<PulsarSynth> pulsarSynthForAuto = std::make_unique<PulsarSynth>(why::PlayModeEnum::Auto);
    std::vector<std::shared_ptr<PulsarSynth>> pulsarSynths;

    //当前触发播放的方式
    why::PlayModeEnum currentPlayModeEnum;

    //All voices under all synth share one impulse source
    std::shared_ptr<ConvolutionResource> convolutionResource;
    //All voices under all synth share one commonVoiceSate
    std::shared_ptr<CommonVoiceSate> commonVoiceSate;

    //TODO  Ready to be discarded
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);
};


#endif //PULSARSYNTHENGINE_H
