#include "DynamicProcessor.h"
#include <cmath>

namespace alr::dsp
{

void DynamicProcessor::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    // Vocal-calibrated ballistics:
    // Fast stage catches consonant/transient spikes (8 ms attack / 65 ms release)
    // Slow opto stage smooths vocal phrasing (35 ms attack / 240 ms release)
    fastAttCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.008f));
    fastRelCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.065f));
    slowAttCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.035f));
    slowRelCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.240f));

    densitySmoother_.prepare(sampleRate_, 15.0f);
    reset();
}

void DynamicProcessor::reset() noexcept
{
    fastEnv_ = 0.0f;
    slowEnv_ = 0.0f;
    currentGrDb_ = 0.0f;
    densitySmoother_.reset(density_);
}

void DynamicProcessor::setVoiceProfile(const VoiceProfile& profile) noexcept
{
    profile_ = profile;
}

void DynamicProcessor::setParameters(float densityAmount, float colorMacro) noexcept
{
    density_ = clampf(densityAmount, 0.0f, 1.0f);
    color_ = clampf(colorMacro, 0.0f, 1.0f);
}

void DynamicProcessor::processBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const float effectiveDensity = clampf(density_ * (0.30f + 0.70f * color_), 0.0f, 1.0f);
    densitySmoother_.setTarget(effectiveDensity);

    // Adapt threshold & ratio conservatively from RELEARN profile if available
    const float baseThreshDb = profile_.isValid ? profile_.targets.autoCompThresholdDb : -18.0f;
    const float baseRatio = profile_.isValid ? profile_.targets.autoCompRatio : 2.2f;

    float minGrDb = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float d = densitySmoother_.process();
        const float inL = left[i];
        const float inR = (right != nullptr) ? right[i] : inL;

        // Stereo-linked detector prevents image shifting
        const float detector = std::max(std::abs(inL), std::abs(inR));

        const float fCoeff = (detector > fastEnv_) ? fastAttCoeff_ : fastRelCoeff_;
        fastEnv_ = killDenormal(detector + fCoeff * (fastEnv_ - detector));

        const float sCoeff = (detector > slowEnv_) ? slowAttCoeff_ : slowRelCoeff_;
        slowEnv_ = killDenormal(detector + sCoeff * (slowEnv_ - detector));

        // Program-dependent hybrid level: 55% opto body + 45% peak transient control
        const float hybridLevel = 0.55f * slowEnv_ + 0.45f * fastEnv_;
        const float levelDb = gainToDb(hybridLevel + 1.0e-6f);

        // Threshold lowers gently and ratio scales smoothly with DENSITY
        const float threshDb = clampf(baseThreshDb - d * 10.0f, -38.0f, -6.0f);
        const float ratio = clampf(baseRatio + d * 2.2f, 1.25f, 5.0f);
        constexpr float kneeDb = 6.0f;

        float grDb = 0.0f;
        const float overDb = levelDb - threshDb;
        if (overDb > -kneeDb * 0.5f)
        {
            const float slope = 1.0f - (1.0f / ratio);
            if (overDb < kneeDb * 0.5f)
            {
                // Smooth quadratic soft knee
                const float x = overDb + kneeDb * 0.5f;
                grDb = -slope * (x * x) / (2.0f * kneeDb);
            }
            else
            {
                grDb = -slope * overDb;
            }
        }

        // Parallel density blend: keeps natural vocal transients alive while bringing up intimate body
        const float compGain = dbToGain(grDb);
        const float autoMakeup = dbToGain(d * 2.8f); // Subtle calibrated makeup for intimate proximity
        const float parallelMix = clampf(0.25f + d * 0.75f, 0.0f, 1.0f);

        float outL = inL;
        float outR = inR;

        if (d > 0.002f)
        {
            const float wetL = inL * compGain * autoMakeup;
            const float wetR = inR * compGain * autoMakeup;
            outL = inL + parallelMix * (wetL - inL);
            outR = inR + parallelMix * (wetR - inR);
            minGrDb = std::min(minGrDb, grDb * parallelMix);
        }

        // Transparent protective soft-clip ceiling at -0.35 dBFS (0.960) so unexpected clipping never occurs
        auto softLimit = [](float x) noexcept -> float
        {
            constexpr float kKnee = 0.85f;
            constexpr float kCeil = 0.9605f; // ~ -0.35 dBFS
            const float ax = std::abs(x);
            if (ax <= kKnee) return x;
            const float excess = (ax - kKnee) / (kCeil - kKnee);
            const float limited = kKnee + (kCeil - kKnee) * std::tanh(excess);
            return (x >= 0.0f) ? limited : -limited;
        };

        left[i] = softLimit(outL);
        if (right != nullptr)
            right[i] = softLimit(outR);
    }

    currentGrDb_ = minGrDb;
}

} // namespace alr::dsp
