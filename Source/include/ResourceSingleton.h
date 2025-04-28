//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

#include "Commons.h"

/**
 * Resources Resource Management: Singleton Pattern
 */
class BinaryResourceSingleton
{
public:
    //Obtain the unique instance
    static BinaryResourceSingleton& getInstance()
    {
        static BinaryResourceSingleton instance;
        return instance;
    }

    // Remove the copy constructor and assignment operator to ensure that only a unique instance can be obtained
    BinaryResourceSingleton(const BinaryResourceSingleton&) = delete;
    BinaryResourceSingleton& operator=(const BinaryResourceSingleton&) = delete;

    /**
     * The binary resource map exposed for external use
     * @return binary resource map
     */
    std::map<juce::String, juce::String> getBinaryIdFileNameMap()
    {
        return binaryIdFileNameMap;
    };

    /**
      * The file names exposed for external use
      * @return file names
      */
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
    //id:value -> (1,2,3,4...):file name
    std::map<juce::String, juce::String> binaryIdFileNameMap;

    BinaryResourceSingleton()
    {
        //塞入BinaryData中所有资源文件名
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* filename = BinaryData::namedResourceList[i];
            binaryIdFileNameMap[static_cast<juce::String>(i + 1)] = filename;
        }
    }
};