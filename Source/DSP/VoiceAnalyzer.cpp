#include "VoiceAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace alr::dsp
{

void VoiceAnalyzer::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    fft_.prepare(kFFTSize);
    reset();
}

void VoiceAnalyzer::reset()
{
    learning_.store(false, std::memory_order_release);
    activeSeconds_.store(0.0f, std::memory_order_release);
    frameRing_.fill(0.0f);
    meanPowerSpectrum_.fill(0.0);
    f0Hist_.fill(0);
    rmsHist_.fill(0);
    f0Count_ = 0;
    rmsCount_ = 0;
    ringWritePos_ = 0;
    hopCounter_ = 0;
    analyzedFrames_ = 0;
    harmonicConfidenceSum_ = 0.0;
    vocalRmsDbSum_ = 0.0;
}

void VoiceAnalyzer::startRelearn() noexcept
{
    frameRing_.fill(0.0f);
    meanPowerSpectrum_.fill(0.0);
    f0Hist_.fill(0);
    rmsHist_.fill(0);
    f0Count_ = 0;
    rmsCount_ = 0;
    ringWritePos_ = 0;
    hopCounter_ = 0;
    analyzedFrames_ = 0;
    harmonicConfidenceSum_ = 0.0;
    vocalRmsDbSum_ = 0.0;
    activeSeconds_.store(0.0f, std::memory_order_release);
    learning_.store(true, std::memory_order_release);
}

void VoiceAnalyzer::stopAndFinalizeIfPossible() noexcept
{
    if (!learning_.load(std::memory_order_acquire))
        return;

    learning_.store(false, std::memory_order_release);
    if (activeSeconds_.load(std::memory_order_acquire) >= kMinValidSeconds && analyzedFrames_ >= 12)
        finalizeProfile();
}

void VoiceAnalyzer::clearProfile() noexcept
{
    learning_.store(false, std::memory_order_release);
    activeSeconds_.store(0.0f, std::memory_order_release);
    profileReady_.store(false, std::memory_order_release);

    const int nextSlot = 1 - activeSlot_.load(std::memory_order_relaxed);
    profileSlots_[nextSlot].reset();
    activeSlot_.store(nextSlot, std::memory_order_release);
    profileJustUpdated_.store(true, std::memory_order_release);
}

float VoiceAnalyzer::binToHz(std::size_t bin) const noexcept
{
    return static_cast<float>(bin) * static_cast<float>(sampleRate_) / static_cast<float>(kFFTSize);
}

std::size_t VoiceAnalyzer::hzToBin(float hz) const noexcept
{
    const float b = hz * static_cast<float>(kFFTSize) / static_cast<float>(sampleRate_);
    return static_cast<std::size_t>(clampf(b, 1.0f, static_cast<float>(kNumBins - 1)));
}

void VoiceAnalyzer::ingestAudio(const float* monoSamples,
                                int numSamples,
                                const VADMetrics& vad,
                                float detectedF0Hz,
                                float f0Confidence) noexcept
{
    if (!learning_.load(std::memory_order_acquire) || monoSamples == nullptr || numSamples <= 0)
        return;

    // Strictly ignore silence, background noise, isolated breaths, and non-vocal segments
    if (!vad.isActiveVocal)
        return;

    const float blockSec = static_cast<float>(numSamples) / static_cast<float>(sampleRate_);
    const float newActiveSec = activeSeconds_.load(std::memory_order_relaxed) + blockSec;
    activeSeconds_.store(std::min(newActiveSec, kMaxActiveSeconds), std::memory_order_release);

    for (int i = 0; i < numSamples; ++i)
    {
        frameRing_[ringWritePos_] = monoSamples[i];
        ringWritePos_ = (ringWritePos_ + 1) & (kFFTSize - 1);
        ++hopCounter_;

        if (hopCounter_ >= kHopSize)
        {
            hopCounter_ = 0;
            // Unwrap ring buffer into contiguous frame
            for (std::size_t k = 0; k < kFFTSize; ++k)
                linearFrame_[k] = frameRing_[(ringWritePos_ + k) & (kFFTSize - 1)];

            analyzeCurrentFrame(detectedF0Hz, f0Confidence, vad.rmsDb);
        }
    }

    if (newActiveSec >= kMaxActiveSeconds)
    {
        learning_.store(false, std::memory_order_release);
        finalizeProfile();
    }
}

void VoiceAnalyzer::analyzeCurrentFrame(float frameF0Hz, float f0Confidence, float frameRmsDb) noexcept
{
    fft_.computeMagnitudeSpectrum(linearFrame_.data(), magSpectrum_.data());

    // Accumulate normalized power spectrum so louder phrases don't completely drown softer ones
    double frameEnergy = 1.0e-12;
    const std::size_t bLow = hzToBin(80.0f);
    const std::size_t bHigh = hzToBin(12000.0f);
    for (std::size_t k = bLow; k <= bHigh; ++k)
        frameEnergy += static_cast<double>(magSpectrum_[k]) * static_cast<double>(magSpectrum_[k]);

    const double invFrameEnergy = 1.0 / frameEnergy;
    for (std::size_t k = 0; k < kNumBins; ++k)
    {
        const double p = static_cast<double>(magSpectrum_[k]) * static_cast<double>(magSpectrum_[k]) * invFrameEnergy;
        meanPowerSpectrum_[k] += p;
    }

    // Track F0 histogram if reliable voiced pitch
    if (frameF0Hz >= 60.0f && frameF0Hz <= 855.0f && f0Confidence >= 0.55f)
    {
        const std::size_t idx = static_cast<std::size_t>(clampf((frameF0Hz - 60.0f) / 5.0f, 0.0f, static_cast<float>(kF0HistBins - 1)));
        ++f0Hist_[idx];
        ++f0Count_;
    }

    // Track RMS dynamic range histogram (-60 .. 0 dBFS)
    if (frameRmsDb > -60.0f && frameRmsDb <= 0.0f)
    {
        const std::size_t rIdx = static_cast<std::size_t>(clampf((frameRmsDb + 60.0f) / 0.625f, 0.0f, static_cast<float>(kRmsHistBins - 1)));
        ++rmsHist_[rIdx];
        ++rmsCount_;
        vocalRmsDbSum_ += static_cast<double>(frameRmsDb);
    }

    harmonicConfidenceSum_ += static_cast<double>(clampf(f0Confidence, 0.0f, 1.0f));
    ++analyzedFrames_;
}

void VoiceAnalyzer::finalizeProfile() noexcept
{
    if (analyzedFrames_ == 0)
        return;

    VoiceProfile p{};
    p.isValid = true;
    p.analyzedActiveSeconds = activeSeconds_.load(std::memory_order_relaxed);
    p.validFrameCount = analyzedFrames_;

    const double invFrames = 1.0 / static_cast<double>(analyzedFrames_);
    for (std::size_t k = 0; k < kNumBins; ++k)
    {
        const double meanP = meanPowerSpectrum_[k] * invFrames;
        avgDbSpectrum_[k] = static_cast<float>(10.0 * std::log10(std::max(meanP, 1.0e-12)));
    }

    // Helper: average band energy in dB
    auto bandPowerDb = [&](float fLo, float fHi) -> float
    {
        const std::size_t b0 = hzToBin(fLo);
        const std::size_t b1 = std::max(b0, hzToBin(fHi));
        double sumP = 0.0;
        for (std::size_t k = b0; k <= b1; ++k)
            sumP += meanPowerSpectrum_[k] * invFrames;
        const double meanPerBin = sumP / static_cast<double>(b1 - b0 + 1);
        return static_cast<float>(10.0 * std::log10(std::max(meanPerBin, 1.0e-12)));
    };

    // Reference core band (200 - 2000 Hz)
    const float refCoreDb = bandPowerDb(200.0f, 2000.0f);

    p.bandRelDb[0] = bandPowerDb(30.0f, 85.0f) - refCoreDb;       // Sub / Rumble
    p.bandRelDb[1] = bandPowerDb(85.0f, 240.0f) - refCoreDb;      // Body
    p.bandRelDb[2] = bandPowerDb(240.0f, 480.0f) - refCoreDb;     // Mud
    p.bandRelDb[3] = bandPowerDb(480.0f, 1100.0f) - refCoreDb;    // Boxy / Nasal
    p.bandRelDb[4] = bandPowerDb(2000.0f, 5000.0f) - refCoreDb;   // Presence
    p.bandRelDb[5] = bandPowerDb(5000.0f, 8500.0f) - refCoreDb;   // Harsh / Sibilance
    p.bandRelDb[6] = bandPowerDb(9000.0f, 16000.0f) - refCoreDb;  // Air

    // F0 Percentiles (P10, P50, P90)
    auto histPercentile = [](const auto& hist, std::uint32_t total, float pct, float minVal, float binStep) -> float
    {
        if (total == 0) return minVal;
        const std::uint32_t target = static_cast<std::uint32_t>(std::max(1.0f, static_cast<float>(total) * pct));
        std::uint32_t acc = 0;
        for (std::size_t i = 0; i < hist.size(); ++i)
        {
            acc += hist[i];
            if (acc >= target)
                return minVal + (static_cast<float>(i) + 0.5f) * binStep;
        }
        return minVal + static_cast<float>(hist.size() - 1) * binStep;
    };

    if (f0Count_ >= 5)
    {
        p.lowF0Hz = histPercentile(f0Hist_, f0Count_, 0.10f, 60.0f, 5.0f);
        p.medianF0Hz = histPercentile(f0Hist_, f0Count_, 0.50f, 60.0f, 5.0f);
        p.highF0Hz = histPercentile(f0Hist_, f0Count_, 0.90f, 60.0f, 5.0f);
    }

    // Dynamic range & average RMS
    if (rmsCount_ >= 5)
    {
        const float p10 = histPercentile(rmsHist_, rmsCount_, 0.10f, -60.0f, 0.625f);
        const float p90 = histPercentile(rmsHist_, rmsCount_, 0.90f, -60.0f, 0.625f);
        p.dynamicRangeDb = clampf(p90 - p10, 2.0f, 36.0f);
        p.averageVocalRmsDb = static_cast<float>(vocalRmsDbSum_ / static_cast<double>(rmsCount_));
    }

    p.harmonicDensity = clampf(static_cast<float>(harmonicConfidenceSum_ * invFrames), 0.1f, 1.0f);

    // Spectral centroid (100 Hz .. 12 kHz)
    double numC = 0.0, denC = 1.0e-12;
    const std::size_t cLo = hzToBin(100.0f);
    const std::size_t cHi = hzToBin(12000.0f);
    for (std::size_t k = cLo; k <= cHi; ++k)
    {
        const double mag = std::sqrt(std::max(meanPowerSpectrum_[k] * invFrames, 0.0));
        numC += static_cast<double>(binToHz(k)) * mag;
        denC += mag;
    }
    p.spectralCentroidHz = clampf(static_cast<float>(numC / denC), 200.0f, 8000.0f);

    // Spectral tilt approximation (dB per octave from 250 Hz to 8000 Hz = 5 octaves)
    p.spectralTiltDbPerOct = (bandPowerDb(4000.0f, 8000.0f) - bandPowerDb(200.0f, 500.0f)) / 4.0f;

    // Smoothed spectral envelope (log-spaced half-octave smoothing) for formant & resonance extraction
    for (std::size_t k = 1; k < kNumBins; ++k)
    {
        const float f = binToHz(k);
        const std::size_t lo = hzToBin(f * 0.72f);
        const std::size_t hi = hzToBin(f * 1.38f);
        float sum = 0.0f;
        for (std::size_t j = lo; j <= hi; ++j)
            sum += avgDbSpectrum_[j];
        smoothEnvDb_[k] = sum / static_cast<float>(hi - lo + 1);
    }

    // Approximate formants F1, F2, F3 (envelope maxima in canonical vocal formant regions)
    auto peakInRange = [&](float fLo, float fHi, float fallback) -> float
    {
        const std::size_t lo = hzToBin(fLo);
        const std::size_t hi = hzToBin(fHi);
        float bestDb = -180.0f;
        std::size_t bestBin = lo;
        for (std::size_t k = lo; k <= hi; ++k)
        {
            if (smoothEnvDb_[k] > bestDb)
            {
                bestDb = smoothEnvDb_[k];
                bestBin = k;
            }
        }
        return (bestDb > -120.0f) ? binToHz(bestBin) : fallback;
    };

    p.formant1Hz = peakInRange(280.0f, 900.0f, 550.0f);
    p.formant2Hz = peakInRange(950.0f, 2500.0f, 1650.0f);
    p.formant3Hz = peakInRange(2500.0f, 4000.0f, 2900.0f);

    // Detect up to 3 distinct narrowband resonance peaks protruding above the local spectral envelope
    struct ResCand { float freq; float excess; };
    std::array<ResCand, 3> topRes{ { { 2600.0f, 0.0f }, { 4300.0f, 0.0f }, { 6800.0f, 0.0f } } };

    const std::size_t rLo = hzToBin(280.0f);
    const std::size_t rHi = hzToBin(7800.0f);
    for (std::size_t k = rLo + 1; k + 1 < rHi; ++k)
    {
        // Local peak check
        if (avgDbSpectrum_[k] > avgDbSpectrum_[k - 1] && avgDbSpectrum_[k] >= avgDbSpectrum_[k + 1])
        {
            const float excess = avgDbSpectrum_[k] - smoothEnvDb_[k];
            if (excess > 2.8f)
            {
                const float f = binToHz(k);
                // Ensure resonances are separated by at least 25% in frequency
                bool merged = false;
                for (auto& c : topRes)
                {
                    if (c.excess > 0.0f && std::abs(f - c.freq) < c.freq * 0.25f)
                    {
                        if (excess > c.excess) { c.freq = f; c.excess = excess; }
                        merged = true;
                        break;
                    }
                }
                if (!merged)
                {
                    // Replace smallest candidate if stronger
                    std::size_t minIdx = 0;
                    for (std::size_t i = 1; i < 3; ++i)
                        if (topRes[i].excess < topRes[minIdx].excess)
                            minIdx = i;
                    if (excess > topRes[minIdx].excess)
                        topRes[minIdx] = { f, excess };
                }
            }
        }
    }

    // Sort detected resonances by frequency ascending for predictable filter assignment
    std::sort(topRes.begin(), topRes.end(), [](const ResCand& a, const ResCand& b) { return a.freq < b.freq; });
    for (int i = 0; i < VoiceProfile::kMaxResonances; ++i)
    {
        p.resonanceFreqHz[i] = clampf(topRes[i].freq, 260.0f, 8200.0f);
        p.resonanceSeverityDb[i] = clampf(topRes[i].excess, 0.0f, 12.0f);
    }

    p.computeAdaptationTargets();

    const int nextSlot = 1 - activeSlot_.load(std::memory_order_relaxed);
    profileSlots_[nextSlot] = p;
    activeSlot_.store(nextSlot, std::memory_order_release);
    profileReady_.store(true, std::memory_order_release);
    profileJustUpdated_.store(true, std::memory_order_release);
}

VoiceProfile VoiceAnalyzer::getProfile() const noexcept
{
    return profileSlots_[activeSlot_.load(std::memory_order_acquire)];
}

void VoiceAnalyzer::setProfile(const VoiceProfile& profile) noexcept
{
    const int nextSlot = 1 - activeSlot_.load(std::memory_order_relaxed);
    profileSlots_[nextSlot] = profile;
    if (profile.isValid)
        profileSlots_[nextSlot].computeAdaptationTargets();
    activeSlot_.store(nextSlot, std::memory_order_release);
    profileReady_.store(profile.isValid, std::memory_order_release);
    activeSeconds_.store(profile.analyzedActiveSeconds, std::memory_order_release);
    profileJustUpdated_.store(true, std::memory_order_release);
}

} // namespace alr::dsp
