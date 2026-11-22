#pragma once

#include "DSPCommon.h"
#include <cstdint>

namespace alr::dsp
{
enum class VoiceState : std::uint8_t
{
    Silence = 0,
    BackgroundNoise = 1,
    Breath = 2,
    UnvoicedConsonant = 3,
    ActiveVoice = 4
};

struct VADMetrics
{
    VoiceState state = VoiceState::Silence;
    bool isActiveVocal = false;       // True for active voiced singing/speech (counts for RELEARN)
    bool isSustainedVowel = false;    // True during stable periodic vowels
    float rmsDb = -96.0f;
    float noiseFloorDb = -65.0f;
    float snrDb = 0.0f;
    float zeroCrossingRate = 0.0f;
    float periodicity = 0.0f;         // 0..1
    float coreToHighRatioDb = 0.0f;
};

class VoiceActivityDetector
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Analyzes a mono block of audio samples (recommended 128..1024 samples).
    // Zero heap allocations; thread-safe when called on a single thread.
    VADMetrics processBlock(const float* monoSamples, int numSamples, float pitchConfidenceHint = -1.0f) noexcept;

    VADMetrics getLastMetrics() const noexcept { return lastMetrics_; }

private:
    float estimatePeriodicity(const float* x, int n) const noexcept;

    double sampleRate_ = 48000.0;
    BiquadFilter coreBandFilter_;   // 120 Hz - 3500 Hz
    BiquadFilter highBandFilter_;   // > 4000 Hz

    float noiseFloorRms_ = 0.0008f; // ~ -62 dBFS initial noise floor
    float hangoverRemainingSec_ = 0.0f;
    float vowelHoldSec_ = 0.0f;
    VADMetrics lastMetrics_{};
};

} // namespace alr::dsp
