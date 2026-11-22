#pragma once

#include "DSPCommon.h"
#include "VoiceProfile.h"

namespace alr::dsp
{
// 2x Oversampled Asymmetric Vocal Harmonic Saturator (WARMTH + Profile warmth bias)
// Generates musical even (2nd) and soft odd (3rd) harmonics with HF pre-de-emphasis
// so vocals gain analog fullness without sibilant fizz or aliasing.
class HarmonicSaturation
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setVoiceProfile(const VoiceProfile& profile) noexcept;
    void setParameters(float warmthAmount, float colorMacro) noexcept;

    void processBlock(float* left, float* right, int numSamples) noexcept;

private:
    inline float saturateSample(float x, float drive, float asym) noexcept;

    double sampleRate_ = 48000.0;
    float warmth_ = 0.25f;
    float color_ = 0.75f;
    float profileWarmthBias_ = 0.0f;

    OnePoleSmoother driveSmoother_;

    // Pre-saturation gentle HF tilt (keeps sibilants clean) & post-saturation DC blocker
    StereoBiquad preTiltFilter_;
    StereoBiquad dcBlocker_;

    // 2x Oversampling anti-imaging / anti-aliasing filters (run at 2 * sampleRate)
    BiquadFilter upFilterL_, upFilterR_;
    BiquadFilter downFilterL_, downFilterR_;
};

} // namespace alr::dsp
