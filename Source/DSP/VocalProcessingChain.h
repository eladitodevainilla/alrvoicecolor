#pragma once

#include "DSPCommon.h"
#include "DynamicProcessor.h"
#include "HarmonicSaturation.h"
#include "PitchCorrector.h"
#include "PitchDetector.h"
#include "ResonanceController.h"
#include "VocalEqualizer.h"
#include "VoiceActivityDetector.h"
#include "VoiceAnalyzer.h"
#include "VoiceProfile.h"
#include <array>
#include <atomic>
#include <vector>

namespace alr::dsp
{
struct ChainParameters
{
    bool bypass = false;
    bool autoGainComp = true;

    // Voice Color
    float body = 0.25f;
    float warmth = 0.20f;
    float presence = 0.25f;
    float air = 0.20f;
    float clarity = 0.25f;
    float density = 0.20f;
    float smooth = 0.25f;
    float color = 0.75f;

    // ALR Pitch
    PitchCorrectorParams pitch{};

    // Output
    float outputGainDb = 0.0f;   // -18 .. +18 dB
    float globalMix = 1.0f;      // 0 .. 1
};

struct ChainTelemetry
{
    float inputPeakDb = -96.0f;
    float inputRmsDb = -96.0f;
    float outputPeakDb = -96.0f;
    float outputRmsDb = -96.0f;
    float gainReductionDb = 0.0f;
    float resonanceCutDb = 0.0f;
    float autoGainAppliedDb = 0.0f;

    VADMetrics vad{};
    PitchTelemetry pitch{};

    bool isRelearning = false;
    bool hasProfile = false;
    float relearnActiveSec = 0.0f;
    float relearnProgress = 0.0f;
};

// Complete 10-Stage Vocal Processing Chain for ALR VOICE COLOR:
//  1. Input Stage (DC block, metering, latency-aligned dry buffer)
//  2. Voice Detection (VAD + RELEARN ingestion)
//  3. Pitch Detection (YIN)
//  4. Automatic Pitch Correction (ALR PITCH)
//  5. Corrective EQ (VoiceProfile adaptive)
//  6. Resonance Control (SMOOTH + learned resonance nodes)
//  7. Harmonic Saturation (WARMTH, 2x oversampled)
//  8. Tonal Enhancement (BODY, CLARITY, PRESENCE, AIR)
//  9. Dynamic Control (DENSITY + protective limiter)
// 10. Output Stage (Auto-Gain Comp, Global Mix, Output Gain, Click-Free Bypass, Metering)
class VocalProcessingChain
{
public:
    static constexpr std::size_t kSpectrumFifoCapacity = 16384;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setParameters(const ChainParameters& params) noexcept;
    const ChainParameters& getParameters() const noexcept { return params_; }

    // RELEARN Controls
    void startRelearn() noexcept { voiceAnalyzer_.startRelearn(); }
    void stopRelearn() noexcept { voiceAnalyzer_.stopAndFinalizeIfPossible(); }
    void resetProfile() noexcept;

    VoiceProfile getVoiceProfile() const noexcept { return voiceAnalyzer_.getProfile(); }
    void setVoiceProfile(const VoiceProfile& profile) noexcept;

    int getLatencySamples() const noexcept { return pitchCorrector_.getLatencySamples(); }

    // Main real-time audio callback (mono or stereo, zero heap allocations!)
    void processBlock(float* left, float* right, int numSamples) noexcept;

    ChainTelemetry getTelemetry() const noexcept { return telemetry_; }

    // EQ curve query for GUI spectrum overlay
    float getEQMagnitudeDb(float freqHz) const noexcept { return vocalEQ_.getCombinedMagnitudeDb(freqHz); }

    // Lock-free pre/post spectrum FIFO taps for GUI thread
    std::size_t popPreSpectrumSamples(float* dest, std::size_t maxCount) noexcept { return preSpecFifo_.pop(dest, maxCount); }
    std::size_t popPostSpectrumSamples(float* dest, std::size_t maxCount) noexcept { return postSpecFifo_.pop(dest, maxCount); }

private:
    void applyProfileToModules(const VoiceProfile& profile) noexcept;

    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;
    ChainParameters params_{};

    // Pre-allocated scratch buffers (zero heap allocations during processBlock)
    std::vector<float> monoScratch_;
    std::vector<float> dryScratchL_;
    std::vector<float> dryScratchR_;

    // Latency-aligned dry delay line (matches PitchCorrector latency for phase-perfect Mix & Bypass)
    static constexpr int kDryDelayCap = 4096;
    static constexpr int kDryDelayMask = kDryDelayCap - 1;
    std::array<float, kDryDelayCap> dryDelayL_{};
    std::array<float, kDryDelayCap> dryDelayR_{};
    int dryDelayWrite_ = 0;

    // Input DC Blocker
    StereoBiquad inputDcBlocker_;

    // Core DSP Modules
    VoiceActivityDetector vad_;
    VoiceAnalyzer voiceAnalyzer_;
    PitchDetector pitchDetector_;
    PitchCorrector pitchCorrector_;
    VocalEqualizer vocalEQ_;
    ResonanceController resonanceCtrl_;
    HarmonicSaturation saturation_;
    DynamicProcessor dynamics_;

    // Smoothers for Output Stage
    OnePoleSmoother outputGainSmoother_;
    OnePoleSmoother globalMixSmoother_;
    OnePoleSmoother bypassWetSmoother_;

    // RMS Auto-Gain Compensation state (matches processed loudness to input for fair A/B)
    float agcInRmsSq_ = 1.0e-6f;
    float agcOutRmsSq_ = 1.0e-6f;
    float agcGainSmoothed_ = 1.0f;

    ChainTelemetry telemetry_{};

    // Lock-free FIFOs for Pre/Post Spectrum Display on UI thread
    LockFreeSPSCQueue<float, kSpectrumFifoCapacity> preSpecFifo_;
    LockFreeSPSCQueue<float, kSpectrumFifoCapacity> postSpecFifo_;
};

} // namespace alr::dsp
