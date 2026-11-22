#include "VocalEqualizer.h"
#include <cmath>

namespace alr::dsp
{

void VocalEqualizer::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
    updateCoefficients();
}

void VocalEqualizer::reset() noexcept
{
    hpfFilter_.reset();
    corrBodyFilter_.reset();
    corrMudFilter_.reset();
    corrNasalFilter_.reset();
    corrPresenceFilter_.reset();
    corrAirFilter_.reset();

    tonalBodyBoost_.reset();
    tonalBodySubCleanup_.reset();
    tonalClarityDip_.reset();
    tonalClarityLift_.reset();
    tonalPresence_.reset();
    tonalAirShelf_.reset();
}

void VocalEqualizer::setVoiceProfile(const VoiceProfile& profile) noexcept
{
    profile_ = profile;
    updateCoefficients();
}

void VocalEqualizer::setParameters(const VocalEQParams& params) noexcept
{
    params_ = params;
}

void VocalEqualizer::updateCoefficients() noexcept
{
    const float colorAmt = clampf(smColor_, 0.0f, 1.0f);
    const bool hasProf = profile_.isValid;

    // -----------------------------------------------------------------------
    // 1. CORRECTIVE STAGE (Adaptive to RELEARN profile, conservative natural defaults)
    // -----------------------------------------------------------------------
    const float hpfHz = hasProf ? profile_.targets.hpfCutoffHz : 72.0f;
    hpfFilter_.setParameters(BiquadType::HighPass, sampleRate_, hpfHz, 0.707f);

    const float profScale = hasProf ? (0.35f + 0.65f * colorAmt) : 0.0f;
    const float bodyAnchorHz = hasProf ? profile_.targets.bodyFreqHz : 185.0f;

    corrBodyFilter_.setParameters(BiquadType::Peak, sampleRate_, bodyAnchorHz, 1.15f,
                                  profile_.targets.correctiveBodyDb * profScale);
    corrMudFilter_.setParameters(BiquadType::Peak, sampleRate_, 310.0f, 1.35f,
                                 profile_.targets.correctiveMudDb * profScale);
    corrNasalFilter_.setParameters(BiquadType::Peak, sampleRate_, 760.0f, 1.6f,
                                   profile_.targets.correctiveNasalDb * profScale);
    corrPresenceFilter_.setParameters(BiquadType::Peak, sampleRate_, 3300.0f, 1.1f,
                                      profile_.targets.correctivePresenceDb * profScale);
    corrAirFilter_.setParameters(BiquadType::HighShelf, sampleRate_, 10500.0f, 0.707f,
                                 profile_.targets.correctiveAirDb * profScale);

    // -----------------------------------------------------------------------
    // 2. TONAL ENHANCEMENT STAGE (Anti-Accumulation Interaction Matrix)
    // -----------------------------------------------------------------------
    const float b = clampf(smBody_, 0.0f, 1.0f);
    const float c = clampf(smClarity_, 0.0f, 1.0f);
    const float p = clampf(smPresence_, 0.0f, 1.0f);
    const float a = clampf(smAir_, 0.0f, 1.0f);

    // Anti-Accumulation Guard: when user pushes multiple tonal controls high simultaneously,
    // gracefully compress total boost intensity so the voice stays musical and balanced.
    const float sumBoostDemand = b + c + p + a;
    const float interactionGuard = (sumBoostDemand > 1.6f)
                                       ? (1.0f / (1.0f + 0.24f * (sumBoostDemand - 1.6f)))
                                       : 1.0f;

    const float tonalScale = colorAmt * interactionGuard;

    // BODY: Rich warmth at singer's body frequency (+0..+4.2 dB) + subtle mud-preventing dip at 315 Hz
    const float bodyGainDb = b * 4.2f * tonalScale;
    const float bodySubCleanupDb = -b * 1.15f * tonalScale;
    tonalBodyBoost_.setParameters(BiquadType::Peak, sampleRate_, bodyAnchorHz, 0.95f, bodyGainDb);
    tonalBodySubCleanup_.setParameters(BiquadType::Peak, sampleRate_, bodyAnchorHz * 1.75f, 1.4f, bodySubCleanupDb);

    // CLARITY: Removes veiled low-mid masking (-0..-3.4 dB @ 380 Hz) + articulation (+0..+2.6 dB @ 2.2 kHz)
    const float clarityDipDb = -c * 3.4f * tonalScale;
    const float clarityLiftDb = c * 2.6f * tonalScale;
    tonalClarityDip_.setParameters(BiquadType::Peak, sampleRate_, 380.0f, 1.25f, clarityDipDb);
    tonalClarityLift_.setParameters(BiquadType::Peak, sampleRate_, 2200.0f, 1.3f, clarityLiftDb);

    // PRESENCE: Forward studio definition (+0..+4.0 dB @ 3.7 kHz), tempered slightly if CLARITY is also high
    const float presenceOverlapTrim = 1.0f - 0.18f * c;
    const float presenceGainDb = p * 4.0f * presenceOverlapTrim * tonalScale;
    tonalPresence_.setParameters(BiquadType::Peak, sampleRate_, 3700.0f, 1.05f, presenceGainDb);

    // AIR: Silky open top-end (+0..+4.6 dB Baxandall high-shelf at 11.4 kHz, avoids 6-8 kHz sibilance)
    const float airGainDb = a * 4.6f * tonalScale;
    tonalAirShelf_.setParameters(BiquadType::HighShelf, sampleRate_, 11400.0f, 0.65f, airGainDb);

    // Gentle loudness compensation so turning up EQ knobs enhances tone rather than merely fooling with volume
    const float approxLoudnessShiftDb = 0.22f * bodyGainDb + 0.12f * clarityLiftDb + 0.10f * clarityDipDb +
                                        0.24f * presenceGainDb + 0.10f * airGainDb;
    tonalCompGain_ = dbToGain(clampf(-approxLoudnessShiftDb * 0.55f, -3.5f, 2.0f));
}

void VocalEqualizer::processCorrectiveBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    // Smooth parameter targets per block (~15 ms time constant)
    const float alpha = 1.0f - std::exp(-static_cast<float>(numSamples) / (static_cast<float>(sampleRate_) * 0.015f));
    smBody_ += alpha * (params_.body - smBody_);
    smClarity_ += alpha * (params_.clarity - smClarity_);
    smPresence_ += alpha * (params_.presence - smPresence_);
    smAir_ += alpha * (params_.air - smAir_);
    smColor_ += alpha * (params_.color - smColor_);

    updateCoefficients();

    for (int i = 0; i < numSamples; ++i)
    {
        float xL = left[i];
        xL = hpfFilter_.processLeft(xL);
        xL = corrBodyFilter_.processLeft(xL);
        xL = corrMudFilter_.processLeft(xL);
        xL = corrNasalFilter_.processLeft(xL);
        xL = corrPresenceFilter_.processLeft(xL);
        xL = corrAirFilter_.processLeft(xL);
        left[i] = xL;

        if (right != nullptr)
        {
            float xR = right[i];
            xR = hpfFilter_.processRight(xR);
            xR = corrBodyFilter_.processRight(xR);
            xR = corrMudFilter_.processRight(xR);
            xR = corrNasalFilter_.processRight(xR);
            xR = corrPresenceFilter_.processRight(xR);
            xR = corrAirFilter_.processRight(xR);
            right[i] = xR;
        }
    }
}

void VocalEqualizer::processTonalBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        float xL = left[i];
        xL = tonalBodyBoost_.processLeft(xL);
        xL = tonalBodySubCleanup_.processLeft(xL);
        xL = tonalClarityDip_.processLeft(xL);
        xL = tonalClarityLift_.processLeft(xL);
        xL = tonalPresence_.processLeft(xL);
        xL = tonalAirShelf_.processLeft(xL);
        left[i] = xL * tonalCompGain_;

        if (right != nullptr)
        {
            float xR = right[i];
            xR = tonalBodyBoost_.processRight(xR);
            xR = tonalBodySubCleanup_.processRight(xR);
            xR = tonalClarityDip_.processRight(xR);
            xR = tonalClarityLift_.processRight(xR);
            xR = tonalPresence_.processRight(xR);
            xR = tonalAirShelf_.processRight(xR);
            right[i] = xR * tonalCompGain_;
        }
    }
}

float VocalEqualizer::getCombinedMagnitudeDb(float freqHz) const noexcept
{
    float db = 0.0f;
    db += hpfFilter_.getMagnitudeDb(sampleRate_, freqHz);
    db += corrBodyFilter_.getMagnitudeDb(sampleRate_, freqHz);
    db += corrMudFilter_.getMagnitudeDb(sampleRate_, freqHz);
    db += corrNasalFilter_.getMagnitudeDb(sampleRate_, freqHz);
    db += corrPresenceFilter_.getMagnitudeDb(sampleRate_, freqHz);
    db += corrAirFilter_.getMagnitudeDb(sampleRate_, freqHz);

    db += tonalBodyBoost_.getMagnitudeDb(sampleRate_, freqHz);
    db += tonalBodySubCleanup_.getMagnitudeDb(sampleRate_, freqHz);
    db += tonalClarityDip_.getMagnitudeDb(sampleRate_, freqHz);
    db += tonalClarityLift_.getMagnitudeDb(sampleRate_, freqHz);
    db += tonalPresence_.getMagnitudeDb(sampleRate_, freqHz);
    db += tonalAirShelf_.getMagnitudeDb(sampleRate_, freqHz);
    return db;
}

} // namespace alr::dsp
