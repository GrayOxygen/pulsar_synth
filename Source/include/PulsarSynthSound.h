//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once
#include "Commons.h"

//控制声音是否输出等
class PulsarSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int midiNoteNumber) override
    {
        if (midiNoteNumber < 60)
        {
            return false;
        }
        return true;
    }

    //--------------------------------------------------------------------------
    bool appliesToChannel(int) override { return true; }
};