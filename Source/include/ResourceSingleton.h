//
// Created by Mr. Wang on 2025/4/20.
//
#ifndef WAVEFORMFACTORY_H
#define WAVEFORMFACTORY_H
#include "Commons.h"

/**
 * 单例模式
 */
class BinaryResourceSingleton
{
public:
    // 获取唯一的WaveformFactory实例
    static BinaryResourceSingleton& getInstance()
    {
        static BinaryResourceSingleton instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值操作符，确保只能通过 getInstance 获取唯一实例
    BinaryResourceSingleton(const BinaryResourceSingleton&) = delete;
    BinaryResourceSingleton& operator=(const BinaryResourceSingleton&) = delete;

    std::map<juce::String, juce::String> getBinaryIdFileNameMap()
    {
        return binaryIdFileNameMap;
    };

    const juce::StringArray getFileNameArray()
    {
        juce::StringArray fileNames;

        for (const auto& pair : getBinaryIdFileNameMap())
        {
            fileNames.add(pair.second);
        }
        return fileNames;
    };

private:
    std::map<juce::String, juce::String> binaryIdFileNameMap;

    // 私有构造函数，确保不能在外部创建实例
    BinaryResourceSingleton()
    {
        // 塞入 BinaryData 中所有资源文件名
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* filename = BinaryData::namedResourceList[i];
            binaryIdFileNameMap[static_cast<juce::String>(i + 1)] = filename;
        }
    }
};
#endif //WAVEFORMFACTORY_H
