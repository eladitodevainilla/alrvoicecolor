#include "VoiceActivityDetector.h"

namespace alr::dsp
{

void VoiceActivityDetector::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    coreBandFilter_.setParameters(BiquadType::BandPass, sampleRate_, 950.0f, 0.55f);
    highBandFilter_.setParameters(BiquadType::HighPass, sampleRate_, 4200.0f, 0.707f);
    reset();
}

void VoiceActivityDetector::reset() noexcept
{
    coreBandFilter_.reset();
    highBandFilter_.reset();
    noiseFloorRms_ = 0.0008f; // ~ -62 dBFS
    hangoverRemainingSec_ = 0.0f;
    vowelHoldSec_ = 0.0f;
    lastMetrics_ = VADMetrics{};
}

float VoiceActivityDetector::estimatePeriodicity(const float* x, int n) const noexcept
{
    if (n < 64) return 0.0f;

    // Check vocal pitch lags from 75 Hz to 750 Hz with decimation for ultra-low CPU
    const int stride = (sampleRate_ > 64000.0) ? 4 : 2;
    const int minLag = std::max(4, static_cast<int>(sampleRate_ / 750.0));
    const int maxLag = std::min(n - 16, static_cast<int>(sampleRate_ / 75.0));
    if (maxLag <= minLag) return 0.0f;

    float bestNormCorr = 0.0f;
    const int stepLag = std::max(1, stride);

    for (int lag = minLag; lag <= maxLag; lag += stepLag)
    {
        float dot = 0.0f;
        float e1 = 0.0f;
        float e2 = 0.0f;
        const int limit = n - lag;
        for (int i = 0; i < limit; i += stride)
        {
            const float a = x[i];
            const float b = x[i + lag];
            dot += a * b;
            e1 += a * a;
            e2 += b * b;
        }
        const float denom = std::sqrt(e1 * e2) + kEpsilon;
        const float corr = dot / denom;
        if (corr > bestNormCorr)
            bestNormCorr = corr;
    }

    return clampf(bestNormCorr, 0.0f, 1.0f);
}

VADMetrics VoiceActivityDetector::processBlock(const float* monoSamples, int numSamples, float pitchConfidenceHint) noexcept
{
    VADMetrics m{};
    if (monoSamples == nullptr || numSamples <= 0)
        return m;

    double sumSq = 0.0;
    double coreSumSq = 0.0;
    double highSumSq = 0.0;
    int zeroCrossings = 0;

    float prev = monoSamples[0];
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = monoSamples[i];
        sumSq += static_cast<double>(s) * static_cast<double>(s);

        const float c = coreBandFilter_.processSample(s);
        const float h = highBandFilter_.processSample(s);
        coreSumSq += static_cast<double>(c) * static_cast<double>(c);
        highSumSq += static_cast<double>(h) * static_cast<double>(h);

        if ((s >= 0.0f && prev < 0.0f) || (s < 0.0f && prev >= 0.0f))
            ++zeroCrossings;
        prev = s;
    }

    const float invN = 1.0f / static_cast<float>(numSamples);
    const float rms = std::sqrt(static_cast<float>(sumSq) * invN);
    const float coreRms = std::sqrt(static_cast<float>(coreSumSq) * invN);
    const float highRms = std::sqrt(static_cast<float>(highSumSq) * invN);

    const float blockSec = static_cast<float>(numSamples) / static_cast<float>(sampleRate_);
    const float zcr = static_cast<float>(zeroCrossings) / std::max(1.0f, static_cast<float>(numSamples - 1));
    const float zcrHz = zcr * static_cast<float>(sampleRate_) * 0.5f;

    const float rmsDb = gainToDb(rms);
    const float coreToHighDb = gainToDb(coreRms + 1.0e-7f) - gainToDb(highRms + 1.0e-7f);

    // Periodicity: use pitch detector confidence when provided, else lightweight autocorr
    const float periodicity = (pitchConfidenceHint >= 0.0f)
                                  ? clampf(pitchConfidenceHint, 0.0f, 1.0f)
                                  : estimatePeriodicity(monoSamples, numSamples);

    // Adaptive noise floor tracking (only tracks downward rapidly, upward very slowly when unvoiced/quiet)
    constexpr float kAbsoluteSilenceDb = -58.0f;
    if (rmsDb > -85.0f)
    {
        if (rms < noiseFloorRms_)
        {
            const float alphaDown = 1.0f - std::exp(-blockSec / 0.15f);
            noiseFloorRms_ += alphaDown * (rms - noiseFloorRms_);
        }
        else if (periodicity < 0.35f && rms < noiseFloorRms_ * 2.5f)
        {
            const float alphaUp = 1.0f - std::exp(-blockSec / 2.5f);
            noiseFloorRms_ += alphaUp * (rms - noiseFloorRms_);
        }
    }
    noiseFloorRms_ = clampf(noiseFloorRms_, 0.00018f, 0.025f); // -75 dBFS to -32 dBFS

    const float noiseFloorDb = gainToDb(noiseFloorRms_);
    const float snrDb = rmsDb - noiseFloorDb;

    // Decision logic separating Silence, Noise, Breath, Unvoiced Consonant, Active Voice
    VoiceState state = VoiceState::Silence;
    bool activeVocalNow = false;
    bool sustainedVowelNow = false;

    if (rmsDb < kAbsoluteSilenceDb)
    {
        state = VoiceState::Silence;
    }
    else if (snrDb < 5.5f && periodicity < 0.45f)
    {
        state = VoiceState::BackgroundNoise;
    }
    else
    {
        // Signal is clearly above noise floor
        const bool strongPeriodicity = (periodicity >= 0.48f);
        const bool strongVocalCore = (coreToHighDb > -2.5f) && (gainToDb(coreRms) > -52.0f);
        const bool highFreqDominant = (coreToHighDb < -3.5f) || (zcrHz > 3200.0f);

        if (strongPeriodicity && strongVocalCore && snrDb >= 6.0f)
        {
            state = VoiceState::ActiveVoice;
            activeVocalNow = true;
            hangoverRemainingSec_ = 0.095f; // 95ms natural phrase hangover

            if (periodicity >= 0.62f && coreToHighDb > 2.0f && zcrHz < 1600.0f)
            {
                vowelHoldSec_ += blockSec;
                sustainedVowelNow = (vowelHoldSec_ >= 0.045f);
            }
            else
            {
                vowelHoldSec_ = 0.0f;
            }
        }
        else if (highFreqDominant && rmsDb > -44.0f)
        {
            // Sharp sibilants / plosives (s, sh, f, t, k) vs soft breath
            if (snrDb > 13.0f || zcrHz > 4200.0f)
                state = VoiceState::UnvoicedConsonant;
            else
                state = VoiceState::Breath;
            vowelHoldSec_ = 0.0f;
        }
        else if (snrDb < 14.0f && periodicity < 0.42f)
        {
            // Soft broad-spectrum air/inhalation
            state = VoiceState::Breath;
            vowelHoldSec_ = 0.0f;
        }
        else if (hangoverRemainingSec_ > 0.0f && snrDb >= 6.0f && strongVocalCore)
        {
            // Brief dip between connected voiced syllables
            state = VoiceState::ActiveVoice;
            activeVocalNow = true;
            hangoverRemainingSec_ = std::max(0.0f, hangoverRemainingSec_ - blockSec);
        }
        else
        {
            state = (snrDb < 9.0f) ? VoiceState::BackgroundNoise : VoiceState::Breath;
            vowelHoldSec_ = 0.0f;
        }
    }

    if (!activeVocalNow)
        hangoverRemainingSec_ = std::max(0.0f, hangoverRemainingSec_ - blockSec);

    m.state = state;
    m.isActiveVocal = activeVocalNow;
    m.isSustainedVowel = sustainedVowelNow;
    m.rmsDb = rmsDb;
    m.noiseFloorDb = noiseFloorDb;
    m.snrDb = snrDb;
    m.zeroCrossingRate = zcrHz;
    m.periodicity = periodicity;
    m.coreToHighRatioDb = coreToHighDb;

    lastMetrics_ = m;
    return m;
}

} // namespace alr::dsp
