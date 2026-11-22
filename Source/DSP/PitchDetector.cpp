#include "PitchDetector.h"
#include <algorithm>
#include <cmath>

namespace alr::dsp
{

void PitchDetector::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);

    // Decimate to ~22-24 kHz analysis rate (Nyquist > 11 kHz: captures F0 up to 920 Hz + rich harmonics)
    decimFactor_ = std::max(1, static_cast<int>(std::round(sampleRate_ / 24000.0)));
    analysisRate_ = sampleRate_ / static_cast<double>(decimFactor_);

    // Anti-aliasing low-pass before decimation (cutoff ~ 0.36 * analysisRate_)
    antiAliasLpf_.setParameters(BiquadType::LowPass, sampleRate_,
                                static_cast<float>(analysisRate_ * 0.36), 0.707f);

    // ~21.3 ms correlation window, ~8.0 ms hop cadence (125 updates/sec)
    windowSize_ = std::max(256, static_cast<int>(std::round(analysisRate_ * 0.021333)));
    hopSize_ = std::max(64, static_cast<int>(std::round(analysisRate_ * 0.008)));

    const int absoluteMaxLag = static_cast<int>(std::ceil(analysisRate_ / 60.0)) + 4;
    bufferSize_ = windowSize_ + absoluteMaxLag;

    ringBuffer_.assign(static_cast<std::size_t>(bufferSize_), 0.0f);
    frameBuffer_.assign(static_cast<std::size_t>(bufferSize_), 0.0f);
    cmndfBuffer_.assign(static_cast<std::size_t>(absoluteMaxLag + 2), 1.0f);

    updateRangeBounds();
    reset();
}

void PitchDetector::reset() noexcept
{
    antiAliasLpf_.reset();
    decimCounter_ = 0;
    std::fill(ringBuffer_.begin(), ringBuffer_.end(), 0.0f);
    std::fill(frameBuffer_.begin(), frameBuffer_.end(), 0.0f);
    f0History_.fill(0.0f);
    writePos_ = 0;
    samplesSinceHop_ = 0;
    histIndex_ = 0;
    consecutiveOctaveJumpFrames_ = 0;
    stableF0Hz_ = 0.0f;
    lastEstimate_ = PitchEstimate{};
}

void PitchDetector::setPitchRange(VocalPitchRange range) noexcept
{
    if (range_ != range)
    {
        range_ = range;
        updateRangeBounds();
    }
}

void PitchDetector::updateRangeBounds() noexcept
{
    switch (range_)
    {
        case VocalPitchRange::Low:  minFreqHz_ = 65.0f;  maxFreqHz_ = 350.0f; break;
        case VocalPitchRange::Mid:  minFreqHz_ = 95.0f;  maxFreqHz_ = 550.0f; break;
        case VocalPitchRange::High: minFreqHz_ = 145.0f; maxFreqHz_ = 920.0f; break;
        case VocalPitchRange::Full:
        default:                    minFreqHz_ = 70.0f;  maxFreqHz_ = 920.0f; break;
    }

    const int maxAllowed = static_cast<int>(cmndfBuffer_.size()) - 2;
    minLag_ = std::max(6, static_cast<int>(std::floor(analysisRate_ / static_cast<double>(maxFreqHz_))));
    maxLag_ = std::min(maxAllowed, static_cast<int>(std::ceil(analysisRate_ / static_cast<double>(minFreqHz_))));
    if (maxLag_ <= minLag_ + 2)
        maxLag_ = std::min(maxAllowed, minLag_ + 4);
}

PitchEstimate PitchDetector::processBlock(const float* monoSamples, int numSamples) noexcept
{
    if (monoSamples == nullptr || numSamples <= 0 || bufferSize_ <= 0)
        return lastEstimate_;

    for (int i = 0; i < numSamples; ++i)
    {
        const float filtered = (decimFactor_ > 1) ? antiAliasLpf_.processSample(monoSamples[i]) : monoSamples[i];
        ++decimCounter_;
        if (decimCounter_ < decimFactor_)
            continue;
        decimCounter_ = 0;

        ringBuffer_[static_cast<std::size_t>(writePos_)] = filtered;
        writePos_ = (writePos_ + 1) % bufferSize_;
        ++samplesSinceHop_;

        if (samplesSinceHop_ >= hopSize_)
        {
            samplesSinceHop_ = 0;

            const int firstPart = bufferSize_ - writePos_;
            for (int k = 0; k < firstPart; ++k)
                frameBuffer_[static_cast<std::size_t>(k)] = ringBuffer_[static_cast<std::size_t>(writePos_ + k)];
            for (int k = 0; k < writePos_; ++k)
                frameBuffer_[static_cast<std::size_t>(firstPart + k)] = ringBuffer_[static_cast<std::size_t>(k)];

            runYinAnalysis();
        }
    }

    return lastEstimate_;
}

void PitchDetector::runYinAnalysis() noexcept
{
    const int analysisW = std::min(windowSize_, bufferSize_ - maxLag_ - 1);
    if (analysisW < 64)
        return;

    float* x = frameBuffer_.data();

    // Remove DC mean & compute base energy E0 = sum_{j=0}^{W-1} x[j]^2
    float mean = 0.0f;
    for (int i = 0; i < bufferSize_; ++i)
        mean += x[i];
    mean /= static_cast<float>(bufferSize_);
    for (int i = 0; i < bufferSize_; ++i)
        x[i] -= mean;

    float energy0 = 0.0f;
    for (int j = 0; j < analysisW; ++j)
        energy0 += x[j] * x[j];

    const float rms = std::sqrt(energy0 / static_cast<float>(analysisW));
    if (rms < 0.002f) // < -54 dBFS
    {
        lastEstimate_.isVoiced = false;
        lastEstimate_.confidence = 0.0f;
        stableF0Hz_ = 0.0f;
        consecutiveOctaveJumpFrames_ = 0;
        return;
    }

    // Step 1: Fast Algebraic Difference Function:
    //   d(tau) = E(0) + E(tau) - 2 * sum_{j=0}^{W-1} x[j] * x[j + tau]
    // Sliding energy E(tau) updates in O(1), and the pure dot product auto-vectorizes via SIMD!
    cmndfBuffer_[0] = 1.0f;
    float runningSum = 0.0f;
    float shiftedEnergy = energy0;

    for (int tau = 1; tau <= maxLag_; ++tau)
    {
        // O(1) sliding energy update for window [tau .. tau + analysisW - 1]
        const float leaving = x[tau - 1];
        const float entering = x[tau + analysisW - 1];
        shiftedEnergy += entering * entering - leaving * leaving;
        if (shiftedEnergy < 0.0f) shiftedEnergy = 0.0f;

        float dot = 0.0f;
        const float* xShift = x + tau;
        for (int j = 0; j < analysisW; ++j)
            dot += x[j] * xShift[j];

        const float d = std::max(0.0f, energy0 + shiftedEnergy - 2.0f * dot);
        runningSum += d;

        if (runningSum > 1.0e-12f)
            cmndfBuffer_[static_cast<std::size_t>(tau)] = (d * static_cast<float>(tau)) / runningSum;
        else
            cmndfBuffer_[static_cast<std::size_t>(tau)] = 1.0f;
    }

    // Step 2: Absolute threshold search (first local minimum below threshold prevents octave-down errors)
    constexpr float kYinThreshold = 0.16f;
    int bestTau = -1;

    for (int tau = minLag_; tau < maxLag_; ++tau)
    {
        if (cmndfBuffer_[static_cast<std::size_t>(tau)] < kYinThreshold)
        {
            while (tau + 1 < maxLag_ &&
                   cmndfBuffer_[static_cast<std::size_t>(tau + 1)] < cmndfBuffer_[static_cast<std::size_t>(tau)])
            {
                ++tau;
            }
            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        float minVal = 1.0f;
        for (int tau = minLag_; tau < maxLag_; ++tau)
        {
            const float v = cmndfBuffer_[static_cast<std::size_t>(tau)];
            if (v < minVal)
            {
                minVal = v;
                bestTau = tau;
            }
        }
        if (minVal > 0.42f || bestTau < minLag_)
        {
            lastEstimate_.isVoiced = false;
            lastEstimate_.confidence = clampf(1.0f - minVal, 0.0f, 1.0f);
            return;
        }
    }

    const float cmndfAtBest = cmndfBuffer_[static_cast<std::size_t>(bestTau)];
    const float confidence = clampf(1.0f - cmndfAtBest, 0.0f, 1.0f);

    // Step 3: Parabolic interpolation for sub-sample precision
    float refinedTau = static_cast<float>(bestTau);
    if (bestTau > minLag_ && bestTau < maxLag_ - 1)
    {
        const float s0 = cmndfBuffer_[static_cast<std::size_t>(bestTau - 1)];
        const float s1 = cmndfBuffer_[static_cast<std::size_t>(bestTau)];
        const float s2 = cmndfBuffer_[static_cast<std::size_t>(bestTau + 1)];
        const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
        if (std::abs(denom) > 1.0e-9f)
            refinedTau += clampf((s0 - s2) / denom, -0.5f, 0.5f);
    }

    float candidateHz = static_cast<float>(analysisRate_) / std::max(1.0f, refinedTau);

    // Step 4: Octave-jump guard
    if (stableF0Hz_ > 1.0f && confidence < 0.94f)
    {
        const float ratio = candidateHz / stableF0Hz_;
        if ((ratio > 1.85f && ratio < 2.15f) || (ratio > 0.46f && ratio < 0.54f))
        {
            ++consecutiveOctaveJumpFrames_;
            if (consecutiveOctaveJumpFrames_ < 3)
            {
                const int expectedLag = static_cast<int>(std::round(analysisRate_ / static_cast<double>(stableF0Hz_)));
                if (expectedLag >= minLag_ && expectedLag < maxLag_ &&
                    cmndfBuffer_[static_cast<std::size_t>(expectedLag)] < 0.28f)
                {
                    candidateHz = stableF0Hz_;
                    refinedTau = static_cast<float>(analysisRate_) / candidateHz;
                }
            }
            else
            {
                consecutiveOctaveJumpFrames_ = 0;
                stableF0Hz_ = candidateHz;
            }
        }
        else
        {
            consecutiveOctaveJumpFrames_ = 0;
            stableF0Hz_ = candidateHz;
        }
    }
    else
    {
        consecutiveOctaveJumpFrames_ = 0;
        stableF0Hz_ = candidateHz;
    }

    f0History_[static_cast<std::size_t>(histIndex_)] = candidateHz;
    histIndex_ = (histIndex_ + 1) % static_cast<int>(f0History_.size());

    lastEstimate_.frequencyHz = candidateHz;
    // Report period in FULL-rate samples for PitchCorrector synchronization
    lastEstimate_.periodSamples = refinedTau * static_cast<float>(decimFactor_);
    lastEstimate_.confidence = confidence;
    lastEstimate_.isVoiced = (confidence >= 0.58f && candidateHz >= minFreqHz_ && candidateHz <= maxFreqHz_);
}

} // namespace alr::dsp
