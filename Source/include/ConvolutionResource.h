//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "BinaryResourceSingleton.h"
#include "Commons.h"
/**
 * All synth and voice share a ConvolutionResource to maintain impulse file data
 * and do some convolution logic When initialized in prepareToPlay, if the
 * sample rate, sample per block, and num channels change, this SynthEngine will
 * reinitialize the ConvolutionResource otherwise keep the instance same.
 */
class ConvolutionResource {
public:
  ConvolutionResource(double sampleRate, int samplesPerBlock, int numChannels) { prepare(sampleRate, samplesPerBlock, numChannels); }

  [[nodiscard]] juce::MemoryBlock &getLastImpulseMemoryBlock1() { return lastImpulseMemoryBlock; }

  [[nodiscard]] std::shared_ptr<juce::dsp::Convolution> &getConvolution() { return convolution; }

  void setConvolution(const std::shared_ptr<juce::dsp::Convolution> &convolution) { this->convolution = convolution; }

  void saveLastTemplateImpulseData(const juce::AudioBuffer<float> &lastTemplateBuffer, double lastTemplateBufferSampleRate, std::string fileName) {
    this->lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(lastTemplateBuffer);
    this->lastTemplateBufferSampleRate = lastTemplateBufferSampleRate;
    this->sampleImpulseFilePath = fileName;
  }

  /**
   * According to the file id, read the impulse file under Resources(binary) and
   * save it to this object (in memory), and then directly load it as impulse
   * response
   *
   * @param selectedId The id of the impulse file combobox, that is, the id of
   * the binary file under Resources
   */
  void saveTemplateImpulse(int selectedId) {
    double fileSampleRate;
    std::unique_ptr<juce::AudioBuffer<float>> bf;
    juce::String resourceName = why::resourceIdToName[selectedId];

    if (resourceName.isNotEmpty() && BinaryResourceSingleton::getInstance().readFileFromResources(resourceName.toRawUTF8(), fileSampleRate, bf)) {
      saveLastTemplateImpulseData(*bf, fileSampleRate, resourceName.toRawUTF8());
      loadTemplateImpulseFile();
    }
  }

  /**
   * Save the file data to this ConvolutionResource object (in memory)
   * @param file file impulse file, such as the impulse file selected from the
   * file selection window
   */
  void saveLastSampleFileAsBlock(const juce::File &file) {
    if (file.getSize() <= 0) {
      return;
    }
    juce::MemoryBlock memBlock;
    file.loadFileAsData(memBlock);
    std::unique_ptr<juce::MemoryBlock> memBlockPtr = std::make_unique<juce::MemoryBlock>(memBlock);
    setLastSampleImpulseMemoryBlock(memBlockPtr);
    this->sampleImpulseFilePath = file.getFullPathName().toStdString();
  }

  bool loadSampleSourceFile(const juce::File &file) {
    if (file.getSize() <= 0) {
      sampleSourceBuffer.setSize(0, 0);
      sampleSourceSampleRate = 0.0;
      sampleSourceReadHead = 0;
      return false;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) {
      sampleSourceBuffer.setSize(0, 0);
      sampleSourceSampleRate = 0.0;
      sampleSourceReadHead = 0;
      return false;
    }

    sampleSourceBuffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&sampleSourceBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    sampleSourceSampleRate = reader->sampleRate;
    sampleSourceReadHead = 0;
    return true;
  }

  /**
   * use saved memory block to load it as impulse response
   */
  void loadSampleImpulseFile() {
    if (getLastSampleImpulseMemoryBlock()->getSize() <= 0) {
      setShouldUseConvolution(false);
      return;
    }
    setShouldUseConvolution(true);

    // real load impulse file
    float irSize = getLastSampleImpulseMemoryBlock()->getSize();
    if (irSize > why::sampleRate) {
      irSize = 2048;
    }
    convolution->loadImpulseResponse(getLastSampleImpulseMemoryBlock()->getData(), getLastSampleImpulseMemoryBlock()->getSize(), juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes,
                                     // TODO There is no limit. If a large file is passed in, it will consume
                                     // a lot of performance. Originally, 1024*8 sample sizes were directly
                                     // fixed, but later it can be changed to allow users to modify them
                                     // manually
                                     0,
                                     juce::dsp::Convolution::Normalise::yes); // 0 = full IR
  }

  /**
   * use template buffer to load it as impulse response
   */
  void loadTemplateImpulseFile() {
    if (lastTemplateBufferSampleRate <= 0 || lastTemplateBuffer->getNumSamples() <= 0) {
      setShouldUseConvolution(false);
      return;
    }
    setShouldUseConvolution(true);

    // Avoid the data inside the pointer becoming empty after the move, as the
    // data in the buffer has been moved
    std::unique_ptr<juce::AudioBuffer<float>> clonedBuffer = std::make_unique<juce::AudioBuffer<float>>(*lastTemplateBuffer);

    // The built-in impulse audio has all been processed and won't be too long
    convolution->loadImpulseResponse(std::move(*clonedBuffer), lastTemplateBufferSampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes,
                                     juce::dsp::Convolution::Normalise::yes);
  }

  /**
   * do convolution logic
   * @param pulseBuffer Data sources that require convolution
   * @return Return the sample after convolution
   */
  bool processSample(juce::AudioBuffer<float> &pulseBuffer) {
    if (!shouldUseConvolution) {
      return false;
    }
    if (convolution->getCurrentIRSize() > 0 && shouldUseConvolution) {
      juce::dsp::AudioBlock<float> block(pulseBuffer);
      juce::dsp::ProcessContextReplacing<float> context(block);
      convolution->process(context);
      return true;
    }
    return false;
  }

  void removeDCOffset(juce::AudioBuffer<float> &buffer) {
    // 遍历每一个声道进行去直流处理
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
      auto *data = buffer.getWritePointer(channel);
      const int numSamples = buffer.getNumSamples();

      // 1. 计算该声道所有采样点的总和
      double sum = 0.0;
      for (int i = 0; i < numSamples; ++i) {
        sum += data[i];
      }

      // 2. 计算平均值（即直流分量 DC Offset）
      float dcOffset = static_cast<float>(sum / numSamples);

      // 3. 将每个采样点减去直流分量，使整体波形以 0 为中心对称
      for (int i = 0; i < numSamples; ++i) {
        data[i] -= dcOffset;
      }
    }
  }
  void buildLinearIR(const juce::AudioBuffer<float> &ringBuffer, int writePos, juce::AudioBuffer<float> &linearIR, int effectiveIRLength) {
    const int channels = ringBuffer.getNumChannels();
    linearIR.setSize(channels, effectiveIRLength, false, false, true);
    for (int ch = 0; ch < channels; ++ch) {
      const float *src = ringBuffer.getReadPointer(ch);
      float *dst = linearIR.getWritePointer(ch);

      const int tailSize = effectiveIRLength - writePos;
      //====================================================
      // 1. copy tail [writePos → end]
      //====================================================
      if (tailSize > 0)
        memcpy(dst, src + writePos, tailSize * sizeof(float));

      //====================================================
      // 2. copy head [0 → writePos]
      //====================================================
      if (writePos > 0)
        memcpy(dst + tailSize, src, writePos * sizeof(float));
    }
  }

  bool processSampleSourceWithPulsarIr(const juce::AudioBuffer<float> &pulsarIrBuffer, juce::AudioBuffer<float> &outputBuffer) {
    // 没有加载 sample source 时，无法把 sample source 当作被卷积的输入信号。
    if (sampleSourceBuffer.getNumSamples() <= 0) {
      return false;
    }

    // outputBuffer 决定本次 audio block 需要处理的声道数和采样数。
    const int numChannels = outputBuffer.getNumChannels();
    const int numSamples = outputBuffer.getNumSamples();
    // sampleSourceBuffer 是循环播放的源素材，声道数可能少于 outputBuffer。
    const int sourceChannels = sampleSourceBuffer.getNumChannels();
    const int sourceSamples = sampleSourceBuffer.getNumSamples();

    // sampleSourceProcessBuffer 保存本次从 sample source 读取出来的 dry input block。
    if (sampleSourceProcessBuffer.getNumChannels() != numChannels || sampleSourceProcessBuffer.getNumSamples() != numSamples) {
      sampleSourceProcessBuffer.setSize(numChannels, numSamples, false, true, true);
    }

    // 从 sampleSourceBuffer 按 sampleSourceReadHead 读取当前 block，并循环回绕。
    for (int channel = 0; channel < numChannels; ++channel) {
      auto *writePtr = sampleSourceProcessBuffer.getWritePointer(channel);
      // 如果输出声道多于 sample source 声道，则复用 sample source 最后一个声道。
      const auto *readPtr = sampleSourceBuffer.getReadPointer(juce::jmin(channel, sourceChannels - 1));
      for (int sample = 0; sample < numSamples; ++sample) {
        writePtr[sample] = readPtr[(sampleSourceReadHead + sample) % sourceSamples];
      }
    }

    sampleSourceReadHead = (sampleSourceReadHead + numSamples) % sourceSamples;

    // pulsar IR 变化时重新加载。loadImpulseResponse 是异步的，JUCE Convolution 内部 overlap-save 会自动保留前一次卷积的 tail，不需要双卷积器或 crossfade.
    if (pulsarIrBuffer.getNumSamples() > 0) {
      juce::AudioBuffer<float> irCopy(pulsarIrBuffer);
      pulsarIrConvolutionA->loadImpulseResponse(std::move(irCopy), spec.sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }

    // 直接卷积：process() 内部 overlap-save 自动延续前一 block 的 tail。
    juce::dsp::AudioBlock<float> block(sampleSourceProcessBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    pulsarIrConvolutionA->process(context);

    // 这样做不仅能彻底消除导致扬声器纸盆剧烈震动的直流偏移（DC Offset），还能完美保留脉冲序列本身的波形形状和节奏 Pattern（即 Curtis Road 所说的次声波律动）。
    // removeDCOffset(const_cast<juce::AudioBuffer<float> &>(sampleSourceProcessBuffer));

    // 将卷积结果?写回调用方传入的 outputBuffer。
    outputBuffer.makeCopyOf(sampleSourceProcessBuffer, true);

    // 直接做乘法，看是否输出0
    // 一行搞定：将 source 和 pulsar 逐元素相乘，结果直接写入 output
    // for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
    //   // 安全地读取 irBuffer 的指针，绝对不要 std::move 一个局部变量
    //   const float *irPtr = irBuffer.getReadPointer(channel);
    //   juce::FloatVectorOperations::multiply(outputBuffer.getWritePointer(channel), sampleSourceProcessBuffer.getReadPointer(channel), irPtr, outputBuffer.getNumSamples());
    // }
    return true;
  }

  [[nodiscard]] std::unique_ptr<juce::MemoryBlock> &getLastSampleImpulseMemoryBlock() { return lastSampleImpulseMemoryBlock; }

  void setLastSampleImpulseMemoryBlock(std::unique_ptr<juce::MemoryBlock> &lastSampleImpulseMemoryBlock) { this->lastSampleImpulseMemoryBlock = std::move(lastSampleImpulseMemoryBlock); }

  [[nodiscard]] std::unique_ptr<juce::AudioBuffer<float>> &getLastTemplateBuffer() { return lastTemplateBuffer; }

  [[nodiscard]] double &getLastTemplateDataSize() { return lastTemplateBufferSampleRate; }

  [[nodiscard]] bool &isShouldUseConvolution() { return shouldUseConvolution; }

  void setShouldUseConvolution(bool shouldUseConvolution) { this->shouldUseConvolution = shouldUseConvolution; }

  [[nodiscard]] juce::MemoryBlock &getLastImpulseMemoryBlock() { return lastImpulseMemoryBlock; }

  void setLastImpulseMemoryBlock(const juce::MemoryBlock &lastImpulseMemoryBlock) { this->lastImpulseMemoryBlock = lastImpulseMemoryBlock; }

private:
  void prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    // config convolution
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = numChannels;
    convolution->prepare(spec);
    pulsarIrConvolutionA->prepare(spec);
  }

  // Used to store the last selected sample impulse file
  std::unique_ptr<juce::MemoryBlock> lastSampleImpulseMemoryBlock = std::make_unique<juce::MemoryBlock>();
  juce::MemoryBlock lastImpulseMemoryBlock;
  std::string sampleImpulseFilePath;

  // Used to store the template impulse file of the last selection
  std::unique_ptr<juce::AudioBuffer<float>> lastTemplateBuffer = std::make_unique<juce::AudioBuffer<float>>(2, 1024);

  double lastTemplateBufferSampleRate = 0;

  // avoid when template impulse is selected but there is no template buffer
  bool shouldUseConvolution = false;

  std::shared_ptr<juce::dsp::Convolution> convolution = std::make_shared<juce::dsp::Convolution>();
  std::shared_ptr<juce::dsp::Convolution> pulsarIrConvolutionA = std::make_shared<juce::dsp::Convolution>();
  juce::dsp::ProcessSpec spec;
  juce::AudioBuffer<float> sampleSourceBuffer;
  juce::AudioBuffer<float> sampleSourceProcessBuffer;
  double sampleSourceSampleRate = 0.0;
  int sampleSourceReadHead = 0;
  static constexpr double irUpdateIntervalMs = 0.0; // 最小 IR 更新间隔（ms）
};
