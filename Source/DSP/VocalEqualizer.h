#pragma once

#include "DSPCommon.h"
#include "VoiceProfile.h"

namespace alr::dsp
{
struct VocalEQParams
{
    float body = 0.25f;      // 0..1 (Low-mid warmth/weight around singer's fundamental)
    float clarity = 0.25f;   // 0..1 (Boxiness removal + upper-mid intelligibility)
    float presence = 0.25f;  // 0..1 (Forward 3-4.5 kHz definition)
    float air = 0.20f;       // 0..1 (Silky >10.5 kHz sheen)
    float color = 0.75f;     // 0..1 (Global color macro scaling both profile & tonal EQ)
};

// Dual-Stage Vocal Equalizer:
//  - Corrective Stage (runs before saturation): Profile-adapted HPF + mud/nasal/tilt correction
//  - Tonal Stage (runs after saturation): Calibrated BODY, CLARITY, PRESENCE, AIR with
//    an Anti-Accumulation Guard that prevents over-boosting when multiple knobs are raised.
class VocalEqualizer
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setVoiceProfile(const VoiceProfile& profile) noexcept;
    void setParameters(const VocalEQParams& params) noexcept;

    void processCorrectiveBlock(float* left, float* right, int numSamples) noexcept;
    void processTonalBlock(float* left, float* right, int numSamples) noexcept;

    // Combined magnitude response in dB for GUI EQ curve visualization
    float getCombinedMagnitudeDb(float freqHz) const noexcept;

private:
    void updateCoefficients() noexcept;

    double sampleRate_ = 48000.0;
    VoiceProfile profile_{};
    VocalEQParams params_{};

    // Smoothed user gains (updated per block to avoid zipper noise while staying lightweight)
    float smBody_ = 0.25f;
    float smClarity_ = 0.25f;
    float smPresence_ = 0.25f;
    float smAir_ = 0.20f;
    float smColor_ = 0.75f;

    // Corrective Stage Filters
    StereoBiquad hpfFilter_;
    StereoBiquad corrBodyFilter_;
    StereoBiquad corrMudFilter_;
    StereoBiquad corrNasalFilter_;
    StereoBiquad corrPresenceFilter_;
    StereoBiquad corrAirFilter_;

    // Tonal Enhancement Stage Filters
    StereoBiquad tonalBodyBoost_;
    StereoBiquad tonalBodySubCleanup_; // Prevents mud buildup when BODY is increased
    StereoBiquad tonalClarityDip_;     // Gentle 360 Hz de-masking scoop
    StereoBiquad tonalClarityLift_;    // 2.2 kHz articulation lift
    StereoBiquad tonalPresence_;       // 3.6 kHz vocal forwardness
    StereoBiquad tonalAirShelf_;       // 11.2 kHz Baxandall-style silky air

    float tonalCompGain_ = 1.0f;       // Subtle gain compensation for tonal energy balance
};

} // namespace alr::dsp
