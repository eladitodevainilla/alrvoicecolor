#pragma once

#include "DSPCommon.h"
#include "VoiceProfile.h"

namespace alr::dsp
{
// Vocal Opto/FET Hybrid Compressor + Protective Soft-Clip Limiter (DENSITY + Profile dynamics)
// Designed for natural studio proximity and vocal consistency without audible pumping.
class DynamicProcessor
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setVoiceProfile(const VoiceProfile& profile) noexcept;
    void setParameters(float densityAmount, float colorMacro) noexcept;

    void processBlock(float* left, float* right, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return currentGrDb_; }

private:
    double sampleRate_ = 48000.0;
    float density_ = 0.25f;
    float color_ = 0.75f;
    VoiceProfile profile_{};

    // Dual-stage envelope detector (fast peak + slow opto RMS for program-dependent release)
    float fastEnv_ = 0.0f;
    float slowEnv_ = 0.0f;
    float fastAttCoeff_ = 0.0f;
    float fastRelCoeff_ = 0.0f;
    float slowAttCoeff_ = 0.0f;
    float slowRelCoeff_ = 0.0f;

    OnePoleSmoother densitySmoother_;
    float currentGrDb_ = 0.0f;
};

} // namespace alr::dsp
