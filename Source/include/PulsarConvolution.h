#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace why {

// ===== Processing Mode Enum =====
enum class PulsarProcessingMode {
  CurtisRoads,  // Event-based: pulsar triggers sample object overlay
  GranularPool  // Grain-based: maintain pool of active grains
};

/**
 * Curtis Roads Pulsar Convolution - Dual implementation
 *
 * "Each pulsar is replaced by a copy of the sampled sound object"
 *
 * Two modes:
 * 1. CurtisRoads: Event-based, immediate overlay (faithful to paper)
 * 2. GranularPool: Maintains grain pool, step-by-step processing
 */
class PulsarConvolution {
public:
  PulsarConvolution();
  ~PulsarConvolution();

  void prepare(double sampleRate, int maxBlockSize);
  void reset();

  /**
   * Process block - grain-based pulsar convolution
   * @param pulsarBuffer 输入 pulsar train (mono)
   * @param sourceBuffer 输入 sample source
   * @param outputBuffer 输出
   * @param numSamples   处理的 samples 数
   */
  void processBlock(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples);

  /**
   * Set sample object (IR) duration in ms
   * Short (< pulsar period) = rhythmic pattern
   * Long (> pulsar period) = continuous texture/filter
   */
  void setSampleObjectDuration(float durationMs);

  /**
   * Set pulsar train fundamental period for trigger optimization
   */
  void setPulsarPeriod(float periodMs);

  /**
   * Set dry/wet mix
   */
  void setMix(float wetAmount); // 0.0 = dry, 1.0 = wet

  /**
   * Set trigger probability to add randomness (0.0-1.0)
   * 1.0 = trigger on every valid pulse (default)
   * 0.5 = 50% chance to trigger
   * 0.1 = 10% chance - sparse grains
   */
  void setTriggerProbability(float probability);

  /**
   * Set noise gate threshold for source audio (dB)
   * Samples below this threshold are treated as silence (-60dB = 0.001 amplitude)
   * This helps eliminate noise floor in the output
   */
  void setNoiseGateThreshold(float thresholdDb);

  /**
   * Set external source buffer (complete sample, not just current block)
   * Grain will read from this buffer to play the full sample object
   */
  void setExternalSource(const juce::AudioBuffer<float> *sourceBuffer, int startIndex = 0);

  /**
   * Set processing mode: CurtisRoads (event-based) or GranularPool
   */
  void setProcessingMode(PulsarProcessingMode mode);

  /**
   * Set output smoothing amount (0.0 = no smoothing, 1.0 = heavy smoothing)
   */
  void setSmoothingAmount(float amount);

private:
  // ===== Grain Structure =====
  struct Grain {
    float amplitude;      // Peak amplitude at trigger (0-1 pulsar value)
    int samplesRemaining; // Samples left to play
    int sourceReadOffset; // Read offset from grain start in external source
    int totalDuration;    // Total grain duration for envelope calculation

    // External source reference (full sample, not ring buffer)
    const juce::AudioBuffer<float> *externalSource;
    int externalSourceStartIndex; // Starting position in external source buffer

    // Simple constructor
    Grain() : amplitude(0), samplesRemaining(0), sourceReadOffset(0), totalDuration(0), externalSource(nullptr), externalSourceStartIndex(0) {}
  };

  // ===== Processing Mode =====
  PulsarProcessingMode processingMode = PulsarProcessingMode::CurtisRoads;

  // ===== Output Smoothing =====
  float smoothingAmount = 0.0f;  // 0.0 = no smoothing, 0.5 = medium, 0.9 = heavy
  float lastOutputSample = 0.0f; // For one-pole lowpass smoothing

  // ===== Grain Pool (for GranularPool mode) =====
  std::vector<Grain> activeGrains;
  static constexpr int MAX_GRAINS = 4;

  // ===== State =====
  juce::Random randomGenerator;

  // External source buffer (complete sample loaded from file)
  const juce::AudioBuffer<float> *externalSourceBuffer = nullptr;
  int externalSourceReadHead = 0; // Current read position in external source
  int externalSourceLength = 0;

  // Source sample buffer (circular for overlap - kept for backwards compatibility)
  juce::AudioBuffer<float> sourceRingBuffer;
  int sourceWriteIndex = 0;
  int sourceBufferSize = 0;

  // Trigger detection
  float lastPulsarValue = 0.0f;
  float peakDetector = 0.0f;
  int samplesSinceLastTrigger = 0;

  // Parameters
  double sampleRate = 44100.0;
  int grainDurationSamples = 44100; // Default 1 second
  float triggerThreshold = 0.05f;
  float minimumTriggerInterval = 0.0f; // Prevent too dense triggers
  float wetMix = 1.0f;
  float dryMix = 0.0f;

  // ===== Internal Methods =====
  void processBlockCurtisRoads(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples);
  void processBlockGranular(const float *pulsarBuffer, juce::AudioBuffer<float> &outputBuffer, int numSamples);
  void spawnGrain(float pulsarAmplitude);
  float processGrain(Grain &grain);
  void triggerPulsar(float pulsarAmplitude, juce::AudioBuffer<float> &outputBuffer, int triggerPos, int numSamples);
  void applyOutputSmoothing(juce::AudioBuffer<float> &outputBuffer, int numSamples);
  bool detectTrigger(float currentPulsar, float prevPulsar);
  float getSourceSample(int index, const juce::AudioBuffer<float> &source);
};

} // namespace why
