#pragma once

#include "DSPCommon.h"
#include "VoiceProfile.h"
#include <array>

namespace alr::dsp
{
// Real-Time 3-Band Dynamic Resonance Suppressor & De-Harsher (SMOOTH control + RELEARN resonances)
// Compares narrowband resonance energy against broadband vocal energy so it attenuates
// protruding harshness/sibilance dynamically without making soft passages dull.
class ResonanceController
{
public:
    static constexpr int kNumBands = 3;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setVoiceProfile(const VoiceProfile& profile) noexcept;
    void setParameters(float smoothAmount, float colorMacro) noexcept;

    void processBlock(float* left, float* right, int numSamples) noexcept;

    // Returns current dynamic attenuation (dB <= 0) for UI feedback
    float getCurrentMaxAttenuationDb() const noexcept { return currentMaxCutDb_; }

private:
    struct DynamicBand
    {
        float centerFreqHz = 3000.0f;
        float q = 3.2f;
        float profileSeverity = 0.0f;
        BiquadFilter sidechainBP;
        StereoBiquad dynamicCut;
        float bandEnv = 0.0f;
        float currentCutDb = 0.0f;
    };

    void configureBands() noexcept;

    double sampleRate_ = 48000.0;
    float smooth_ = 0.25f;
    float color_ = 0.75f;
    VoiceProfile profile_{};

    std::array<DynamicBand, kNumBands> bands_{};
    float broadbandEnv_ = 0.0f;
    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;
    float currentMaxCutDb_ = 0.0f;
};

} // namespace alr::dsp
