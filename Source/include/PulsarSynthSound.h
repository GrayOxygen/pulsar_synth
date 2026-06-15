#pragma once

/**
 * Custom sound: Only notes with midiNoteNumber>=60 are allowed to trigger
 * sounds
 */
class PulsarSynthSound : public juce::SynthesiserSound {
public:
  bool appliesToNote(int midiNoteNumber) override {
    // in ableton that is equal or higher than C3 note
    if (midiNoteNumber < 60) {
      return false;
    }
    return true;
  }

  //--------------------------------------------------------------------------
  bool appliesToChannel(int) override { return true; }
};
