#pragma once

#include "DSPCommon.h"
#include <array>
#include <cstdint>
#include <vector>

namespace alr::dsp
{
enum class VocalPitchRange : int
{
    Low = 0,   // Bass / Low Baritone: 65 - 350 Hz
    Mid = 1,   // Baritone / Tenor / Alto: 95 - 550 Hz
    High = 2,  // Alto / Soprano: 145 - 920 Hz
    Full = 3   // Full Vocal Range: 70 - 920 Hz
};

struct PitchEstimate
{
    float frequencyHz = 0.0f;
    float periodSamples = 0.0f; // Period in FULL-rate samples
    float confidence = 0.0f;    // 0..1 (1 = perfectly periodic)
    bool isVoiced = false;
};

// High-Efficiency Real-Time YIN Fundamental Frequency Estimator
// Uses anti-aliased decimation (target ~24 kHz analysis rate) + O(1) sliding energy
// + auto-vectorized inner product + parabolic interpolation + octave-jump protection.
// Zero heap allocations after prepare().
class PitchDetector
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void setPitchRange(VocalPitchRange range) noexcept;
    VocalPitchRange getPitchRange() const noexcept { return range_; }

    PitchEstimate processBlock(const float* monoSamples, int numSamples) noexcept;
    PitchEstimate getLastEstimate() const noexcept { return lastEstimate_; }

private:
    void runYinAnalysis() noexcept;
    void updateRangeBounds() noexcept;

    double sampleRate_ = 48000.0;
    int decimFactor_ = 2;
    double analysisRate_ = 24000.0;

    int windowSize_ = 512;      // Analysis window at analysisRate_ (~21.3 ms)
    int bufferSize_ = 1024;     // windowSize_ + absoluteMaxLag
    int hopSize_ = 192;         // ~8 ms hop at analysisRate_ (~125 Hz tracking rate)

    VocalPitchRange range_ = VocalPitchRange::Full;
    float minFreqHz_ = 70.0f;
    float maxFreqHz_ = 920.0f;
    int minLag_ = 26;
    int maxLag_ = 343;

    BiquadFilter antiAliasLpf_;
    int decimCounter_ = 0;

    std::vector<float> ringBuffer_;
    std::vector<float> frameBuffer_;
    std::vector<float> cmndfBuffer_;

    int writePos_ = 0;
    int samplesSinceHop_ = 0;

    // Octave-jump protection history
    std::array<float, 5> f0History_{};
    int histIndex_ = 0;
    int consecutiveOctaveJumpFrames_ = 0;
    float stableF0Hz_ = 0.0f;

    PitchEstimate lastEstimate_{};
};

} // namespace alr::dsp
