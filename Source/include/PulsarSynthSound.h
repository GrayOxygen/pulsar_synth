//
// Created by Mr. Wang on 2025/4/20.
//
#pragma once

/**
 * 自定义sound：仅允许midiNoteNumber>=60的note触发声音
 */
class PulsarSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int midiNoteNumber) override
    {
        //in ableton that is equal or higher than C3 note
        if (midiNoteNumber < 60)
        {
            return false;
        }
        return true;
    }

    //--------------------------------------------------------------------------
    bool appliesToChannel(int) override { return true; }
};
