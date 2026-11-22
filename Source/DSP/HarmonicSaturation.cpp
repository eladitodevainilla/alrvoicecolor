#include "HarmonicSaturation.h"
#include <cmath>

namespace alr::dsp
{

void HarmonicSaturation::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    driveSmoother_.prepare(sampleRate_, 15.0f);

    // Gentle -2.5 dB shelf above 6.5 kHz into the nonlinear stage prevents harsh sibilant distortion
    preTiltFilter_.setParameters(BiquadType::HighShelf, sampleRate_, 6500.0f, 0.707f, -2.5f);
    dcBlocker_.setParameters(BiquadType::HighPass, sampleRate_, 22.0f, 0.707f);

    // 2x oversampling filters operating at 2 * sampleRate, cutoff at 0.42 * sampleRate
    const double osRate = sampleRate_ * 2.0;
    const float cutoff = static_cast<float>(sampleRate_ * 0.42);
    upFilterL_.setParameters(BiquadType::LowPass, osRate, cutoff, 0.707f);
    upFilterR_.setParameters(BiquadType::LowPass, osRate, cutoff, 0.707f);
    downFilterL_.setParameters(BiquadType::LowPass, osRate, cutoff, 0.707f);
    downFilterR_.setParameters(BiquadType::LowPass, osRate, cutoff, 0.707f);

    reset();
}

void HarmonicSaturation::reset() noexcept
{
    preTiltFilter_.reset();
    dcBlocker_.reset();
    upFilterL_.reset();
    upFilterR_.reset();
    downFilterL_.reset();
    downFilterR_.reset();
    driveSmoother_.reset(0.0f);
}

void HarmonicSaturation::setVoiceProfile(const VoiceProfile& profile) noexcept
{
    profileWarmthBias_ = profile.isValid ? profile.targets.autoWarmthBias : 0.0f;
}

void HarmonicSaturation::setParameters(float warmthAmount, float colorMacro) noexcept
{
    warmth_ = clampf(warmthAmount, 0.0f, 1.0f);
    color_ = clampf(colorMacro, 0.0f, 1.0f);
}

inline float HarmonicSaturation::saturateSample(float x, float drive, float asym) noexcept
{
    // Asymmetric soft tube/tape transfer function:
    //  - Bias introduces warm 2nd-order even harmonics
    //  - tanh introduces smooth 3rd-order tape compression
    //  - Normalized by small-signal derivative so loudness stays unity-matched
    const float driven = x * drive + asym;
    const float shaped = std::tanh(driven) - std::tanh(asym);
    const float norm = 1.0f / std::max(1.0f, drive * (1.0f - std::tanh(asym) * std::tanh(asym)));
    return shaped * norm;
}

void HarmonicSaturation::processBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const float effectiveWarmth = clampf((warmth_ + profileWarmthBias_ * 0.7f) * color_, 0.0f, 1.0f);
    driveSmoother_.setTarget(effectiveWarmth);

    if (effectiveWarmth < 0.002f && driveSmoother_.getCurrent() < 0.002f)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float w = driveSmoother_.process();
        // Musical calibrated drive: 1.0 (clean) -> 3.6 (rich warm analog saturation, never fizzy fuzz)
        const float drive = 1.0f + w * 2.6f;
        const float asym = w * 0.22f;
        const float blend = clampf(w * 0.85f, 0.0f, 0.85f);

        // Left channel (2x oversampled zero-stuffed + LPF -> nonlinearity -> LPF -> decimate)
        const float dryL = left[i];
        const float shapedInL = preTiltFilter_.processLeft(dryL);

        const float upL0 = upFilterL_.processSample(shapedInL * 2.0f);
        const float upL1 = upFilterL_.processSample(0.0f);
        const float nlL0 = saturateSample(upL0, drive, asym);
        const float nlL1 = saturateSample(upL1, drive, asym);
        const float dnL0 = downFilterL_.processSample(nlL0);
        (void) downFilterL_.processSample(nlL1);

        const float wetL = dcBlocker_.processLeft(dnL0);
        left[i] = dryL + blend * (wetL - dryL);

        // Right channel
        if (right != nullptr)
        {
            const float dryR = right[i];
            const float shapedInR = preTiltFilter_.processRight(dryR);

            const float upR0 = upFilterR_.processSample(shapedInR * 2.0f);
            const float upR1 = upFilterR_.processSample(0.0f);
            const float nlR0 = saturateSample(upR0, drive, asym);
            const float nlR1 = saturateSample(upR1, drive, asym);
            const float dnR0 = downFilterR_.processSample(nlR0);
            (void) downFilterR_.processSample(nlR1);

            const float wetR = dcBlocker_.processRight(dnR0);
            right[i] = dryR + blend * (wetR - dryR);
        }
    }
}

} // namespace alr::dsp
