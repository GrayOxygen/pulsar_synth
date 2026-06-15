//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include "Commons.h"

/**
 * Binary Resources Management: Singleton Pattern
 */
class BinaryResourceSingleton {
public:
  // Obtain the unique instance
  static BinaryResourceSingleton &getInstance() {
    static BinaryResourceSingleton instance;
    return instance;
  }

  // Remove the copy constructor and assignment operator to ensure that only a unique instance can be obtained
  BinaryResourceSingleton(const BinaryResourceSingleton &) = delete;
  BinaryResourceSingleton &operator=(const BinaryResourceSingleton &) = delete;

  /**
   * The file names exposed for external use
   * @return file names
   */
  const juce::StringArray getFileNameArray() {
    juce::StringArray fileNames;

    for (const std::pair<int, juce::String> &entry : why::resourceIdToName) {
      fileNames.add(entry.second);
    }
    return fileNames;
  };

  /**
   * read binary source file
   * @param resourceName resource full name
   * @param sampleRate sample rate of the file as result
   * @param bf buffer
   * @return true is success false failed
   */
  bool readFileFromResources(const char *resourceName, double &sampleRate, std::unique_ptr<juce::AudioBuffer<float>> &bf) {
    // 读取template文件
    //  int dataSize = binaryFileNameSizeMap[resourceName];
    //  const void* data = BinaryData::getNamedResource(resourceName, dataSize);
    if (!binaryFileNameSizeMap.contains(resourceName)) {
      return false;
    }
    std::pair<int, const void *> binaryFilePair = binaryFileNameSizeMap[resourceName];
    // data 是二进制数据的起始地址，dataSize 是它的大小
    // if (data != nullptr)
    // {
    std::unique_ptr<juce::MemoryInputStream> stream;
    stream.reset(new juce::MemoryInputStream(binaryFilePair.second, binaryFilePair.first, false));
    // 例如加载成 AudioBuffer
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats(); // 支持 WAV, AIFF 等常见格式

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(stream)));

    if (reader != nullptr) {
      juce::AudioBuffer<float> buffer(reader->numChannels, static_cast<int>(reader->lengthInSamples));
      reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
      // 现在 buffer 就是你加载好的 impulse data

      bf = std::make_unique<juce::AudioBuffer<float>>(buffer);
      sampleRate = reader->sampleRate;
    }
    return true;
    // }
    // return false;
  }

private:
  // id:value -> file name1:pair(1024,fileData)；By default, a map is sorted in alphabetical order
  std::map<juce::String, std::pair<int, const void *>> binaryFileNameSizeMap;

  BinaryResourceSingleton() {
    // 塞入BinaryData中所有资源文件名
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
      const char *filename = BinaryData::originalFilenames[i];

      int dataSize = 0;
      const void *data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSize);
      // data == nullptr means file is not found
      if (data != nullptr) {
        binaryFileNameSizeMap[filename] = std::make_pair(dataSize, data);
      }
    }
  }
};