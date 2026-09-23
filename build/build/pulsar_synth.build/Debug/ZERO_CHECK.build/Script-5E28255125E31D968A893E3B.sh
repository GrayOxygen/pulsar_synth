#!/bin/sh
set -e
if test "$CONFIGURATION" = "Debug"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  make -f /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "Release"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  make -f /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "MinSizeRel"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  make -f /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "RelWithDebInfo"; then :
  cd /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build
  make -f /Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/CMakeScripts/ReRunCMake.make
fi

