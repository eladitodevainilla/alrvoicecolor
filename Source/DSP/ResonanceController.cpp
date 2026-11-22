#include "ResonanceController.h"
#include <cmath>

namespace alr::dsp
{

void ResonanceController::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    attackCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.004f));
    releaseCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.055f));
    configureBands();
    reset();
}

void ResonanceController::reset() noexcept
{
    broadbandEnv_ = 0.0f;
    currentMaxCutDb_ = 0.0f;
    for (auto& b : bands_)
    {
        b.sidechainBP.reset();
        b.dynamicCut.reset();
        b.bandEnv = 0.0f;
        b.currentCutDb = 0.0f;
    }
}

void ResonanceController::setVoiceProfile(const VoiceProfile& profile) noexcept
{
    profile_ = profile;
    configureBands();
}

void ResonanceController::setParameters(float smoothAmount, float colorMacro) noexcept
{
    smooth_ = clampf(smoothAmount, 0.0f, 1.0f);
    color_ = clampf(colorMacro, 0.0f, 1.0f);
}

void ResonanceController::configureBands() noexcept
{
    const std::array<float, kNumBands> defaultFreqs{ 2550.0f, 4250.0f, 6800.0f };
    const std::array<float, kNumBands> defaultQs{ 2.8f, 3.0f, 2.4f };

    for (int i = 0; i < kNumBands; ++i)
    {
        if (profile_.isValid && profile_.resonanceSeverityDb[static_cast<std::size_t>(i)] > 2.5f)
        {
            bands_[static_cast<std::size_t>(i)].centerFreqHz = clampf(profile_.resonanceFreqHz[static_cast<std::size_t>(i)], 280.0f, 8200.0f);
            bands_[static_cast<std::size_t>(i)].q = 3.5f;
            bands_[static_cast<std::size_t>(i)].profileSeverity = profile_.resonanceSeverityDb[static_cast<std::size_t>(i)];
        }
        else
        {
            bands_[static_cast<std::size_t>(i)].centerFreqHz = defaultFreqs[static_cast<std::size_t>(i)];
            bands_[static_cast<std::size_t>(i)].q = defaultQs[static_cast<std::size_t>(i)];
            bands_[static_cast<std::size_t>(i)].profileSeverity = 0.0f;
        }

        bands_[static_cast<std::size_t>(i)].sidechainBP.setParameters(
            BiquadType::BandPass, sampleRate_,
            bands_[static_cast<std::size_t>(i)].centerFreqHz,
            bands_[static_cast<std::size_t>(i)].q);
    }
}

void ResonanceController::processBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const float profileBias = profile_.isValid ? profile_.targets.autoSmoothBias : 0.0f;
    const float effectiveSmooth = clampf((smooth_ + profileBias * 0.65f) * (0.25f + 0.75f * color_), 0.0f, 1.0f);

    if (effectiveSmooth < 0.005f)
    {
        currentMaxCutDb_ = 0.0f;
        return;
    }

    constexpr int kControlInterval = 32;
    float worstCut = 0.0f;

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min(kControlInterval, numSamples - offset);

        for (int i = 0; i < chunk; ++i)
        {
            const int idx = offset + i;
            const float mono = (right != nullptr) ? 0.5f * (left[idx] + right[idx]) : left[idx];
            const float absMono = std::abs(mono);

            const float bbCoeff = (absMono > broadbandEnv_) ? attackCoeff_ : releaseCoeff_;
            broadbandEnv_ = killDenormal(absMono + bbCoeff * (broadbandEnv_ - absMono));

            for (auto& b : bands_)
            {
                const float bp = std::abs(b.sidechainBP.processSample(mono));
                const float c = (bp > b.bandEnv) ? attackCoeff_ : releaseCoeff_;
                b.bandEnv = killDenormal(bp + c * (b.bandEnv - bp));
            }
        }

        // Frequency-aware natural vocal slope compensation:
        // Upper-mid / sibilant bands naturally carry lower absolute amplitude than the fundamental,
        // so scaling the ratio gently by frequency catches harsh vocal spikes accurately!
        const float bbRef = broadbandEnv_ + 0.0012f;
        for (auto& b : bands_)
        {
            const float freqWeight = clampf(b.centerFreqHz / 1800.0f, 0.8f, 3.2f);
            const float normalizedRatio = (b.bandEnv * freqWeight) / bbRef;

            const float thresh = 0.34f - 0.24f * effectiveSmooth;
            const float excess = std::max(0.0f, normalizedRatio - thresh);

            const float sevBoost = 1.0f + clampf(b.profileSeverity * 0.08f, 0.0f, 0.55f);
            const float targetCutDb = clampf(-excess * 16.0f * effectiveSmooth * sevBoost, -7.5f, 0.0f);

            b.currentCutDb += 0.35f * (targetCutDb - b.currentCutDb);
            b.dynamicCut.setParameters(BiquadType::Peak, sampleRate_, b.centerFreqHz, b.q, b.currentCutDb);
            worstCut = std::min(worstCut, b.currentCutDb);
        }

        for (int i = 0; i < chunk; ++i)
        {
            const int idx = offset + i;
            float xL = left[idx];
            for (auto& b : bands_)
                xL = b.dynamicCut.processLeft(xL);
            left[idx] = xL;

            if (right != nullptr)
            {
                float xR = right[idx];
                for (auto& b : bands_)
                    xR = b.dynamicCut.processRight(xR);
                right[idx] = xR;
            }
        }

        offset += chunk;
    }

    currentMaxCutDb_ = worstCut;
}

} // namespace alr::dsp
