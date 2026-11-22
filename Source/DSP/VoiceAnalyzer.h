#pragma once

#include "DSPCommon.h"
#include "VoiceActivityDetector.h"
#include "VoiceProfile.h"
#include <array>
#include <atomic>
#include <vector>

namespace alr::dsp
{
// RELEARN Engine: Analyzes up to 40.0 seconds of ACTIVE vocal signal (ignoring silence,
// noise, and breaths) and builds a personalized VoiceProfile.
class VoiceAnalyzer
{
public:
    static constexpr std::size_t kFFTSize = 2048;
    static constexpr std::size_t kHopSize = 512;
    static constexpr std::size_t kNumBins = kFFTSize / 2 + 1;
    static constexpr float kMaxActiveSeconds = 40.0f;
    static constexpr float kMinValidSeconds = 2.0f;

    void prepare(double sampleRate);
    void reset();

    // User control (call from UI or processor control path)
    void startRelearn() noexcept;
    void stopAndFinalizeIfPossible() noexcept;
    void clearProfile() noexcept;

    // Audio-thread safe: ingests mono samples + current VAD + detected F0.
    // Only pushes active vocal audio into the pre-allocated FFT frame buffer;
    // performs at most 1 FFT frame per hop (very low CPU, zero heap allocations).
    void ingestAudio(const float* monoSamples,
                     int numSamples,
                     const VADMetrics& vad,
                     float detectedF0Hz,
                     float f0Confidence) noexcept;

    bool isLearning() const noexcept { return learning_.load(std::memory_order_acquire); }
    float getActiveSecondsAccumulated() const noexcept { return activeSeconds_.load(std::memory_order_acquire); }
    float getProgressNormalized() const noexcept { return clampf(getActiveSecondsAccumulated() / kMaxActiveSeconds, 0.0f, 1.0f); }

    bool hasCompletedProfile() const noexcept { return profileReady_.load(std::memory_order_acquire); }

    // Thread-safe snapshot copy of the finalized profile
    VoiceProfile getProfile() const noexcept;
    void setProfile(const VoiceProfile& profile) noexcept;

    // Returns true once after RELEARN finishes (for notifying processor/DSP to apply new profile)
    bool consumeProfileUpdatedFlag() noexcept { return profileJustUpdated_.exchange(false, std::memory_order_acq_rel); }

private:
    void analyzeCurrentFrame(float frameF0Hz, float f0Confidence, float frameRmsDb) noexcept;
    void finalizeProfile() noexcept;
    float binToHz(std::size_t bin) const noexcept;
    std::size_t hzToBin(float hz) const noexcept;

    double sampleRate_ = 48000.0;
    Radix2FFT fft_;

    // Pre-allocated analysis buffers (zero audio-thread allocations)
    std::array<float, kFFTSize> frameRing_{};
    std::array<float, kFFTSize> linearFrame_{};
    std::array<float, kNumBins> magSpectrum_{};
    std::array<double, kNumBins> meanPowerSpectrum_{};
    std::array<float, kNumBins> avgDbSpectrum_{};
    std::array<float, kNumBins> smoothEnvDb_{};

    // Histogram-based percentile tracking (fixed memory, zero sorting/allocs during ingest!)
    static constexpr std::size_t kF0HistBins = 160;   // 60 Hz .. 860 Hz (5 Hz resolution)
    static constexpr std::size_t kRmsHistBins = 96;   // -60 dBFS .. 0 dBFS (0.625 dB resolution)
    std::array<std::uint32_t, kF0HistBins> f0Hist_{};
    std::array<std::uint32_t, kRmsHistBins> rmsHist_{};
    std::uint32_t f0Count_ = 0;
    std::uint32_t rmsCount_ = 0;

    std::size_t ringWritePos_ = 0;
    std::size_t hopCounter_ = 0;
    std::uint32_t analyzedFrames_ = 0;
    double harmonicConfidenceSum_ = 0.0;
    double vocalRmsDbSum_ = 0.0;

    std::atomic<bool> learning_{false};
    std::atomic<bool> profileReady_{false};
    std::atomic<bool> profileJustUpdated_{false};
    std::atomic<float> activeSeconds_{0.0f};

    // Double-buffered profile swap (lock-free read/write)
    VoiceProfile profileSlots_[2]{};
    std::atomic<int> activeSlot_{0};
};

} // namespace alr::dsp
