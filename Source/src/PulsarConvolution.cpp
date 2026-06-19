#include "../include/PulsarConvolution.h"
#include <algorithm>
#include <cmath>

namespace why {

PulsarConvolution::PulsarConvolution() = default;

PulsarConvolution::~PulsarConvolution() = default;

void PulsarConvolution::prepare(double sr, int maxBlockSize) {
  sampleRate = sr;
  // Initialize ring buffer for source samples
  sourceBufferSize = static_cast<int>(sampleRate * 2.0); // 2 seconds max
  sourceRingBuffer.setSize(2, sourceBufferSize);
  sourceRingBuffer.clear();
  sourceWriteIndex = 0;

  // Reset state
  lastPulsarValue = 0.0f;
  peakDetector = 0.0f;
  samplesSinceLastTrigger = 0;
  activeGrains.clear();
}

void PulsarConvolution::reset() {
  sourceWriteIndex = 0;
  sourceRingBuffer.clear();
  lastPulsarValue = 0.0f;
  peakDetector = 0.0f;
  samplesSinceLastTrigger = 0;
  activeGrains.clear();
}

void PulsarConvolution::setSampleObjectDuration(float durationMs) {
  grainDurationSamples = static_cast<int>((durationMs / 1000.0f) * sampleRate);
  grainDurationSamples = juce::jmin(grainDurationSamples, sourceBufferSize);
}

void PulsarConvolution::setPulsarPeriod(float periodMs) {
  // Calculate minimum trigger interval based on period
  minimumTriggerInterval = static_cast<int>((periodMs / 1000.0f) * sampleRate);

  // Adjust threshold based on frequency
  if (periodMs < 50.0f) {
    triggerThreshold = 0.03f; // Higher threshold for dense pulses
  } else {
    triggerThreshold = 0.01f; // Lower threshold for infrasonic
  }
}

void PulsarConvolution::setMix(float wetAmount) {
  wetMix = juce::jlimit(0.0f, 1.0f, wetAmount);
  dryMix = 1.0f - wetMix;
}

void PulsarConvolution::setTriggerProbability(float probability) {
  // Simplified: no longer used in Curtis Roads implementation
  juce::ignoreUnused(probability);
}

void PulsarConvolution::setNoiseGateThreshold(float thresholdDb) {
  // Simplified: no longer used in Curtis Roads implementation
  juce::ignoreUnused(thresholdDb);
}

void PulsarConvolution::setExternalSource(const juce::AudioBuffer<float> *sourceBuffer, int startIndex) {
  externalSourceBuffer = sourceBuffer;
  externalSourceReadHead = startIndex;
  if (sourceBuffer != nullptr) {
    externalSourceLength = sourceBuffer->getNumSamples();
  }
}

bool PulsarConvolution::detectTrigger(float currentPulsar, float prevPulsar) {
  // Amplitude-based peak detection
  // Trigger when pulsar value peaks (rising then falling)

  bool rising = currentPulsar > prevPulsar;

  samplesSinceLastTrigger++;

  // Track peak value during rising phase
  if (rising) {
    peakDetector = currentPulsar;
  }

  // Trigger on falling edge after peak, with minimum interval to prevent double-triggering
  if (!rising && peakDetector > triggerThreshold && samplesSinceLastTrigger > minimumTriggerInterval) {
    peakDetector = 0.0f;
    samplesSinceLastTrigger = 0;
    return true;
  }

  return false;
}

float PulsarConvolution::getSourceSample(int index, const juce::AudioBuffer<float> &source) {
  // Read from ring buffer with wrap
  int ringIndex = (index + sourceBufferSize) % sourceBufferSize;

  // Interpolate if we have multi-channel source
  if (source.getNumChannels() > 1) {
    float left = sourceRingBuffer.getSample(0, ringIndex);
    float right = sourceRingBuffer.getSample(1, ringIndex);
    return (left + right) * 0.5f;
  }
  return sourceRingBuffer.getSample(0, ringIndex);
}

void PulsarConvolution::processBlock(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples) {
  const int numChannels = outputBuffer.getNumChannels();

  // ===== Mode Selection =====
  if (processingMode == PulsarProcessingMode::CurtisRoads) {
    processBlockCurtisRoads(pulsarBuffer, outputBuffer, numSamples);
  } else {
    processBlockGranular(pulsarBuffer, outputBuffer, numSamples);
  }
}

void PulsarConvolution::processBlockCurtisRoads(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples) {
  const int numChannels = outputBuffer.getNumChannels();

  // ===== Curtis Roads Pulsar Convolution: Event-Based =====
  // "Each pulsar is replaced by a copy of the sampled sound object"

  for (int i = 0; i < numSamples; ++i) {
    // ===== STEP 1: Detect pulsar trigger =====
    float prevPulsar = (i == 0) ? lastPulsarValue : pulsarBuffer[i - 1];
    float currentPulsar = pulsarBuffer[i];

    if (detectTrigger(currentPulsar, prevPulsar)) {
      // ===== STEP 2: Trigger = Copy sample object to output =====
      triggerPulsar(currentPulsar, outputBuffer, i, numSamples);
    }

    lastPulsarValue = currentPulsar;
  }

  // ===== STEP 3: Apply output smoothing =====
  applyOutputSmoothing(outputBuffer, numSamples);
}

void PulsarConvolution::processBlockGranular(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples) {
  const int numChannels = outputBuffer.getNumChannels();

  for (int i = 0; i < numSamples; ++i) {
    // ===== STEP 1: Detect pulsar triggers =====
    float prevPulsar = (i == 0) ? lastPulsarValue : pulsarBuffer[i - 1];
    float currentPulsar = pulsarBuffer[i];

    if (detectTrigger(currentPulsar, prevPulsar)) {
      spawnGrain(1.0f);
    }

    lastPulsarValue = currentPulsar;

    // ===== STEP 2: Process all active grains =====
    float wetSample = 0.0f;
    int activeGrainCount = 0;

    for (size_t g = 0; g < activeGrains.size();) {
      float grainOutput = processGrain(activeGrains[g]);
      wetSample += grainOutput;
      activeGrainCount++;

      if (activeGrains[g].samplesRemaining <= 0) {
        activeGrains[g] = activeGrains.back();
        activeGrains.pop_back();
      } else {
        ++g;
      }
    }

    // Adaptive normalization
    if (activeGrainCount > 1) {
      wetSample *= 0.15f;
      // wetSample = std::tanh(wetSample);
    }
    wetSample = juce::jlimit(-2.0f, 2.0f, wetSample);

    // Get dry sample
    float drySample = 0.0f;
    if (externalSourceBuffer != nullptr) {
      int dryReadPos = (externalSourceReadHead + i) % externalSourceLength;
      int extChannels = externalSourceBuffer->getNumChannels();
      for (int ch = 0; ch < extChannels && ch < numChannels; ++ch) {
        drySample += externalSourceBuffer->getSample(ch, dryReadPos);
      }
      drySample /= juce::jmax(1, extChannels);
    }

    float finalSample = wetSample * wetMix + drySample * dryMix;

    for (int ch = 0; ch < numChannels; ++ch) {
      outputBuffer.setSample(ch, i, finalSample);
    }
  }

  if (externalSourceBuffer != nullptr) {
    externalSourceReadHead = (externalSourceReadHead + numSamples) % externalSourceLength;
  }

  // Apply output smoothing
  applyOutputSmoothing(outputBuffer, numSamples);
}

void PulsarConvolution::applyOutputSmoothing(juce::AudioBuffer<float> &outputBuffer, int numSamples) {
  if (smoothingAmount <= 0.0f || smoothingAmount >= 1.0f) {
    return; // No smoothing or invalid amount
  }

  const int numChannels = outputBuffer.getNumChannels();
  // One-pole lowpass: y[n] = (1-a) * y[n-1] + a * x[n]
  // where a = smoothingAmount
  float coeff = smoothingAmount;
  float oneMinusCoeff = 1.0f - coeff;

  for (int ch = 0; ch < numChannels; ++ch) {
    float *channelData = outputBuffer.getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i) {
      lastOutputSample = oneMinusCoeff * lastOutputSample + coeff * channelData[i];
      channelData[i] = lastOutputSample;
    }
  }
}

void PulsarConvolution::triggerPulsar(float pulsarAmplitude, juce::AudioBuffer<float> &outputBuffer, int triggerPos, int numSamples) {
  if (externalSourceBuffer == nullptr || externalSourceLength <= 0) {
    return;
  }

  // Pick random start position in sample
  int randomOffset = randomGenerator.nextInt(juce::jmin(1000, externalSourceLength));
  int sampleStartIndex = (externalSourceReadHead + randomOffset) % externalSourceLength;

  // Overlay sample object to output with Blackman-Harris window
  const int duration = juce::jmin(grainDurationSamples, externalSourceLength);
  const int numChannels = outputBuffer.getNumChannels();

  for (int pos = 0; pos < duration; ++pos) {
    int outputPos = triggerPos + pos;
    if (outputPos >= numSamples)
      break; // Don't exceed buffer

    // Read from external source
    int sourceIdx = (sampleStartIndex + pos) % externalSourceLength;
    float sample = 0.0f;
    int extCh = externalSourceBuffer->getNumChannels();
    for (int ch = 0; ch < extCh && ch < numChannels; ++ch) {
      sample += externalSourceBuffer->getSample(ch, sourceIdx);
    }
    sample /= juce::jmax(1, extCh);

    // Blackman-Harris window envelope
    float phase = static_cast<float>(pos) / static_cast<float>(duration - 1);
    const float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
    float envelope =
        a0 - a1 * std::cos(2.0f * juce::MathConstants<float>::pi * phase) + a2 * std::cos(4.0f * juce::MathConstants<float>::pi * phase) - a3 * std::cos(6.0f * juce::MathConstants<float>::pi * phase);

    // Add to output (convolution: pulsar replaced by sample object)
    float wetSample = sample * envelope * pulsarAmplitude * wetMix;

    for (int ch = 0; ch < numChannels; ++ch) {
      float current = outputBuffer.getSample(ch, outputPos);
      outputBuffer.setSample(ch, outputPos, current + wetSample);
    }
  }
}

// ===== Mode and Smoothing Setters =====
void PulsarConvolution::setProcessingMode(PulsarProcessingMode mode) { processingMode = mode; }

void PulsarConvolution::setSmoothingAmount(float amount) { smoothingAmount = juce::jlimit(0.0f, 0.99f, amount); }

// ===== Granular Pool Methods (Legacy) =====
void PulsarConvolution::spawnGrain(float pulsarAmplitude) {
  // Fixed Grain Pool: Silently drop if at capacity
  if (activeGrains.size() >= MAX_GRAINS) {
    return; // Drop this trigger gracefully
  }

  // Handle short samples: limit grain duration to available source length
  int availableSamples = externalSourceLength;
  int actualGrainDuration = grainDurationSamples;
  if (availableSamples > 0 && actualGrainDuration > availableSamples) {
    actualGrainDuration = availableSamples; // Don't loop short samples
  }

  Grain grain;
  grain.amplitude = pulsarAmplitude;
  grain.samplesRemaining = actualGrainDuration;
  grain.totalDuration = actualGrainDuration;   // For Blackman-Harris window calculation
  grain.sourceReadOffset = 0;                  // Start from beginning of grain
  grain.externalSource = externalSourceBuffer; // Pointer to full sample
  // Random scan: pick random start position in sample source
  if (externalSourceLength > 0) {
    int randomOffset = randomGenerator.nextInt(5000); // 附近为位置随机
    grain.externalSourceStartIndex = (externalSourceReadHead + randomOffset) % externalSourceLength;
  } else {
    grain.externalSourceStartIndex = 0;
  }

  activeGrains.push_back(grain);
}

float PulsarConvolution::processGrain(Grain &grain) {
  if (grain.samplesRemaining <= 0 || grain.totalDuration <= 0) {
    return 0.0f;
  }

  // ===== Time-Domain Grain Reading =====
  float sample = 0.0f;
  if (grain.externalSource != nullptr) {
    int readPos = grain.externalSourceStartIndex + grain.sourceReadOffset;
    int sourceLength = grain.externalSource->getNumSamples();
    readPos = readPos % sourceLength;

    // Mix down channels for mono output
    int numCh = grain.externalSource->getNumChannels();
    for (int ch = 0; ch < numCh; ++ch) {
      sample += grain.externalSource->getSample(ch, readPos);
    }
    sample /= juce::jmax(1, numCh);
  }

  // ===== Blackman-Harris Window (Better than Hann: -92dB sidelobes vs -31dB) =====
  // Calculate position in grain (0.0 to 1.0)
  int currentPos = grain.totalDuration - grain.samplesRemaining;
  float phase = (grain.totalDuration > 1) ? static_cast<float>(currentPos) / static_cast<float>(grain.totalDuration - 1) : 0.0f;

  // 4-term Blackman-Harris coefficients
  const float a0 = 0.35875f;
  const float a1 = 0.48829f;
  const float a2 = 0.14128f;
  const float a3 = 0.01168f;

  float envelope =
      a0 - a1 * std::cos(2.0f * juce::MathConstants<float>::pi * phase) + a2 * std::cos(4.0f * juce::MathConstants<float>::pi * phase) - a3 * std::cos(6.0f * juce::MathConstants<float>::pi * phase);

  // Apply envelope and amplitude
  sample *= envelope * grain.amplitude;

  // Advance
  grain.sourceReadOffset++;
  grain.samplesRemaining--;

  return sample;
}

} // namespace why
