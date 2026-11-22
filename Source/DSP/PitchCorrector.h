#pragma once

#include "DSPCommon.h"
#include "PitchDetector.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace alr::dsp
{
// All 17 enharmonic root selections required by ALR PITCH
enum class RootNoteOption : int
{
    C = 0,
    CSharp = 1,
    Db = 2,
    D = 3,
    DSharp = 4,
    Eb = 5,
    E = 6,
    F = 7,
    FSharp = 8,
    Gb = 9,
    G = 10,
    GSharp = 11,
    Ab = 12,
    A = 13,
    ASharp = 14,
    Bb = 15,
    B = 16
};

enum class ScaleType : int
{
    Major = 0,
    Minor = 1
};

struct PitchCorrectorParams
{
    bool enabled = false;
    RootNoteOption rootNote = RootNoteOption::C;
    ScaleType scaleType = ScaleType::Major;
    float retuneSpeedMs = 35.0f;    // 0 ms (fast modern) .. 200 ms (natural slow)
    float humanize = 0.35f;         // 0..1 (preserves vibrato & sustained expression)
    float flexTransition = 0.30f;   // 0..1 (softens note-to-note transitions & glides)
    VocalPitchRange pitchRange = VocalPitchRange::Full;
    float mix = 1.0f;               // 0..1 (phase-aligned dry/wet)
};

struct PitchTelemetry
{
    float detectedHz = 0.0f;
    float targetHz = 0.0f;
    int detectedMidiNote = -1;
    int targetMidiNote = -1;
    float inputCentsOffset = 0.0f;      // -50 .. +50 cents relative to nearest chromatic
    float appliedCorrectionCents = 0.0f; // Actual shift currently applied in cents
    bool isVoiced = false;
};

// Real-Time Pitch-Synchronous Dual-Grain Overlap-Add (PSOLA-style) Pitch Corrector
// - Phase-coherent crossfade synchronized to the detected vocal fundamental period T0
// - Exact musical scale quantization (Major / Minor across all 12 chromatic pitch classes)
// - Vibrato-preserving Humanize + Glide-aware Flex/Transition
// - Unvoiced consonant pass-through (prevents metallic sibilants)
// - Phase-aligned dry/wet mixing with deterministic latency reporting
class PitchCorrector
{
public:
    static int rootOptionToPitchClass(RootNoteOption opt) noexcept;
    static bool isPitchClassInScale(int pitchClass, RootNoteOption root, ScaleType scale) noexcept;
    static int quantizeMidiNoteToScale(float continuousMidi,
                                       RootNoteOption root,
                                       ScaleType scale,
                                       int previousTargetMidi,
                                       float hysteresisCents = 16.0f) noexcept;

    void prepare(double sampleRate);
    void reset() noexcept;

    void setParameters(const PitchCorrectorParams& params) noexcept { params_ = params; }
    const PitchCorrectorParams& getParameters() const noexcept { return params_; }

    // Reported deterministic latency in samples for DAW delay compensation
    int getLatencySamples() const noexcept { return nominalLatencySamples_; }

    // Processes mono or stereo block in-place (pass right == nullptr for mono)
    void processBlock(float* left,
                      float* right,
                      int numSamples,
                      const PitchEstimate& pitch) noexcept;

    PitchTelemetry getTelemetry() const noexcept { return telemetry_; }

private:
    static constexpr int kBufSize = 8192;
    static constexpr int kBufMask = kBufSize - 1;

    float readInterpolated(const std::array<float, kBufSize>& buf, float fractionalPos) const noexcept;
    void updateTargetRatio(const PitchEstimate& pitch, int numSamples) noexcept;

    double sampleRate_ = 48000.0;
    int nominalLatencySamples_ = 512; // ~10.6 ms at 48 kHz (scaled for higher SR)

    PitchCorrectorParams params_{};

    // Stereo circular history buffers (pre-allocated, zero heap usage during audio)
    std::array<float, kBufSize> delayBufL_{};
    std::array<float, kBufSize> delayBufR_{};
    int writeIdx_ = 0;

    // Synchronized dual-grain read heads
    float phase_ = 0.0f;       // [0, 1)
    float delayA_ = 512.0f;
    float delayB_ = 512.0f;
    float latchedHalfSpan_ = 512.0f;
    float currentPeriodSamples_ = 240.0f; // Default ~200 Hz

    // Pitch tracking, vibrato & transition smoothing
    int lastTargetMidi_ = -1;
    float sustainedNoteSec_ = 0.0f;
    float prevDetectedMidi_ = 0.0f;
    float slowPitchTrackerMidi_ = 0.0f;
    float currentRatio_ = 1.0f;
    float targetRatio_ = 1.0f;

    OnePoleSmoother mixSmoother_;
    OnePoleSmoother enableSmoother_;
    PitchTelemetry telemetry_{};
};

} // namespace alr::dsp
