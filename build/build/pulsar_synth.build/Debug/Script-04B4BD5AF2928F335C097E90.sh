#!/bin/sh
set -e
if test "$CONFIGURATION" = "Debug"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/tools/extras/Build/juceaide/juceaide_artefacts/Debug/juceaide header /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/Debug/Defs.txt /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/JuceHeader.h
fi
if test "$CONFIGURATION" = "Release"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/tools/extras/Build/juceaide/juceaide_artefacts/Debug/juceaide header /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/Release/Defs.txt /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/JuceHeader.h
fi
if test "$CONFIGURATION" = "MinSizeRel"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/tools/extras/Build/juceaide/juceaide_artefacts/Debug/juceaide header /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/MinSizeRel/Defs.txt /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/JuceHeader.h
fi
if test "$CONFIGURATION" = "RelWithDebInfo"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/tools/extras/Build/juceaide/juceaide_artefacts/Debug/juceaide header /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/RelWithDebInfo/Defs.txt /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/pulsar_synth_artefacts/JuceLibraryCode/JuceHeader.h
fi

