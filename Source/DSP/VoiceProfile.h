#pragma once

#include "DSPCommon.h"
#include <array>
#include <cstdint>

namespace alr::dsp
{
// Learned vocal profile produced by RELEARN (up to 40s of active vocal audio)
struct VoiceProfile
{
    static constexpr int kNumBands = 7;
    static constexpr int kMaxResonances = 3;

    bool isValid = false;
    float analyzedActiveSeconds = 0.0f;
    std::uint32_t validFrameCount = 0;

    // Pitch & Register metrics
    float medianF0Hz = 165.0f;
    float lowF0Hz = 110.0f;      // ~10th percentile
    float highF0Hz = 260.0f;     // ~90th percentile

    // 7-Band mean spectral energy relative to mid vocal core (dB)
    // 0: Sub/Rumble (<85Hz)
    // 1: Body/Low (85-240Hz)
    // 2: Mud (240-480Hz)
    // 3: Boxy/Nasal (480-1100Hz)
    // 4: Presence (2000-5000Hz)
    // 5: Harsh/Sibilance (5000-8500Hz)
    // 6: Air (9000-16000Hz)
    std::array<float, kNumBands> bandRelDb{ -24.0f, 0.0f, -2.0f, -4.0f, -8.0f, -15.0f, -22.0f };

    // Approximate formant centroids (Hz)
    float formant1Hz = 550.0f;
    float formant2Hz = 1650.0f;
    float formant3Hz = 2850.0f;

    // Spectral & Timbral descriptors
    float spectralCentroidHz = 1400.0f;
    float spectralTiltDbPerOct = -4.5f;
    float harmonicDensity = 0.65f;   // 0..1 (higher = richer/cleaner voiced harmonics)
    float dynamicRangeDb = 11.0f;    // RMS P90 - P10 across active vocal phrases
    float averageVocalRmsDb = -18.0f;

    // Top 3 detected problematic resonances protruding above local spectral envelope
    std::array<float, kMaxResonances> resonanceFreqHz{ 2600.0f, 4300.0f, 6800.0f };
    std::array<float, kMaxResonances> resonanceSeverityDb{ 0.0f, 0.0f, 0.0f };

    // Derived Conservative DSP Targets (computed automatically on finalize)
    struct AdaptationTargets
    {
        float hpfCutoffHz = 75.0f;
        float bodyFreqHz = 180.0f;
        float correctiveBodyDb = 0.0f;       // -2.5 to +2.5 dB
        float correctiveMudDb = 0.0f;        // -3.5 to 0.0 dB
        float correctiveNasalDb = 0.0f;      // -3.0 to 0.0 dB
        float correctivePresenceDb = 0.0f;   // -2.0 to +2.5 dB
        float correctiveAirDb = 0.0f;        // -1.5 to +2.5 dB
        float autoWarmthBias = 0.0f;         // 0.0 to 0.35 (extra gentle harmonic richness if thin)
        float autoSmoothBias = 0.0f;         // 0.0 to 0.45 (extra de-harshing if resonant)
        float autoCompThresholdDb = -18.0f;  // Adapted to singer's average RMS
        float autoCompRatio = 2.0f;          // 1.4 to 3.2 (conservative!)
    } targets;

    void reset() noexcept
    {
        *this = VoiceProfile{};
    }

    // Computes conservative, natural vocal processing targets from measured acoustics.
    // Philosophy: NEVER assume louder, brighter, or heavier compression is better.
    void computeAdaptationTargets() noexcept
    {
        if (!isValid || validFrameCount < 10)
        {
            targets = AdaptationTargets{};
            return;
        }

        // 1. High-pass cutoff safely below the singer's lowest notes (protect fundamental!)
        targets.hpfCutoffHz = clampf(lowF0Hz * 0.68f, 52.0f, 115.0f);

        // 2. Dynamic body anchor around the singer's vocal fundamental warmth zone
        targets.bodyFreqHz = clampf(medianF0Hz * 1.15f, 125.0f, 250.0f);

        // Reference balanced studio vocal relative band profile (empirical target in dB rel core)
        constexpr float kRefBodyDb = -1.0f;
        constexpr float kRefMudDb = -3.5f;
        constexpr float kRefNasalDb = -5.5f;
        constexpr float kRefPresenceDb = -9.5f;
        constexpr float kRefAirDb = -21.0f;

        // 3. Conservative corrective EQ (never aggressive; max ~3.5 dB cut, ~2.5 dB boost)
        const float bodyDiff = kRefBodyDb - bandRelDb[1];
        targets.correctiveBodyDb = clampf(bodyDiff * 0.32f, -2.5f, 2.5f);

        // Only cut mud if mud energy exceeds balanced reference
        const float mudExcess = bandRelDb[2] - kRefMudDb;
        targets.correctiveMudDb = (mudExcess > 0.8f) ? clampf(-mudExcess * 0.45f, -3.5f, 0.0f) : 0.0f;

        // Only tame nasal/boxy buildup if protruding
        const float nasalExcess = bandRelDb[3] - kRefNasalDb;
        targets.correctiveNasalDb = (nasalExcess > 1.0f) ? clampf(-nasalExcess * 0.38f, -3.0f, 0.0f) : 0.0f;

        // Presence: lift subtly if dull, attenuate gently if overly forward/harsh
        const float presDiff = kRefPresenceDb - bandRelDb[4];
        targets.correctivePresenceDb = clampf(presDiff * 0.28f, -2.2f, 2.5f);

        // Air: gently open dark recordings, never force harshness on bright mics
        const float airDiff = kRefAirDb - bandRelDb[6];
        targets.correctiveAirDb = clampf(airDiff * 0.22f, -1.5f, 2.5f);

        // 4. Harmonic saturation bias: add gentle even warmth if vocal sounds thin/sparse
        const float thinness = clampf((0.68f - harmonicDensity) * 0.8f + (bodyDiff > 1.5f ? 0.12f : 0.0f), 0.0f, 0.35f);
        targets.autoWarmthBias = thinness;

        // 5. Smooth/resonance bias based on measured resonance severity & sibilance band
        float maxResSev = 0.0f;
        for (float s : resonanceSeverityDb)
            maxResSev = std::max(maxResSev, s);
        const float harshExcess = std::max(0.0f, bandRelDb[5] - (-14.0f));
        targets.autoSmoothBias = clampf((maxResSev * 0.045f) + (harshExcess * 0.03f), 0.0f, 0.45f);

        // 6. Adaptive dynamics: match threshold to singer's real RMS; gentle ratio if already even
        targets.autoCompThresholdDb = clampf(averageVocalRmsDb - 2.5f, -34.0f, -8.0f);
        if (dynamicRangeDb > 15.0f)
            targets.autoCompRatio = 3.0f;
        else if (dynamicRangeDb > 10.0f)
            targets.autoCompRatio = 2.3f;
        else if (dynamicRangeDb > 6.5f)
            targets.autoCompRatio = 1.8f;
        else
            targets.autoCompRatio = 1.35f; // Already consistent: preserve micro-dynamics
    }
};

} // namespace alr::dsp
