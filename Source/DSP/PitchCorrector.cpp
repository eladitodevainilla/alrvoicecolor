#include "PitchCorrector.h"
#include <algorithm>
#include <cmath>

namespace alr::dsp
{

int PitchCorrector::rootOptionToPitchClass(RootNoteOption opt) noexcept
{
    switch (opt)
    {
        case RootNoteOption::C:      return 0;
        case RootNoteOption::CSharp:
        case RootNoteOption::Db:     return 1;
        case RootNoteOption::D:      return 2;
        case RootNoteOption::DSharp:
        case RootNoteOption::Eb:     return 3;
        case RootNoteOption::E:      return 4;
        case RootNoteOption::F:      return 5;
        case RootNoteOption::FSharp:
        case RootNoteOption::Gb:     return 6;
        case RootNoteOption::G:      return 7;
        case RootNoteOption::GSharp:
        case RootNoteOption::Ab:     return 8;
        case RootNoteOption::A:      return 9;
        case RootNoteOption::ASharp:
        case RootNoteOption::Bb:     return 10;
        case RootNoteOption::B:      return 11;
        default:                     return 0;
    }
}

bool PitchCorrector::isPitchClassInScale(int pitchClass, RootNoteOption root, ScaleType scale) noexcept
{
    const int rootPc = rootOptionToPitchClass(root);
    const int interval = ((pitchClass - rootPc) % 12 + 12) % 12;

    if (scale == ScaleType::Major)
    {
        // W W H W W W H -> 0, 2, 4, 5, 7, 9, 11
        return interval == 0 || interval == 2 || interval == 4 || interval == 5 ||
               interval == 7 || interval == 9 || interval == 11;
    }
    else
    {
        // Natural Minor: W H W W H W W -> 0, 2, 3, 5, 7, 8, 10
        return interval == 0 || interval == 2 || interval == 3 || interval == 5 ||
               interval == 7 || interval == 8 || interval == 10;
    }
}

int PitchCorrector::quantizeMidiNoteToScale(float continuousMidi,
                                            RootNoteOption root,
                                            ScaleType scale,
                                            int previousTargetMidi,
                                            float hysteresisCents) noexcept
{
    const int baseMidi = static_cast<int>(std::round(continuousMidi));
    int bestNote = baseMidi;
    float bestDist = 1000.0f;

    // Search +/- 3 semitones around continuousMidi for closest allowed scale tone
    for (int m = baseMidi - 3; m <= baseMidi + 3; ++m)
    {
        const int pc = ((m % 12) + 12) % 12;
        if (isPitchClassInScale(pc, root, scale))
        {
            const float dist = std::abs(continuousMidi - static_cast<float>(m));
            if (dist < bestDist)
            {
                bestDist = dist;
                bestNote = m;
            }
        }
    }

    // Apply note-boundary hysteresis to prevent fluttering when hovering halfway between two scale notes
    if (previousTargetMidi >= 0 && previousTargetMidi != bestNote)
    {
        const int prevPc = ((previousTargetMidi % 12) + 12) % 12;
        if (isPitchClassInScale(prevPc, root, scale))
        {
            const float distPrev = std::abs(continuousMidi - static_cast<float>(previousTargetMidi));
            const float hysteresisSemitones = hysteresisCents * 0.01f;
            if (distPrev <= bestDist + hysteresisSemitones)
                return previousTargetMidi;
        }
    }

    return bestNote;
}

void PitchCorrector::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    // ~10.6 ms at 48 kHz (512 samples), scaled cleanly for high sample rates
    nominalLatencySamples_ = std::clamp(static_cast<int>(std::round(sampleRate_ * 0.0106667)), 256, 2048);

    mixSmoother_.prepare(sampleRate_, 12.0f);
    enableSmoother_.prepare(sampleRate_, 10.0f);
    reset();
}

void PitchCorrector::reset() noexcept
{
    delayBufL_.fill(0.0f);
    delayBufR_.fill(0.0f);
    writeIdx_ = 0;
    phase_ = 0.0f;
    latchedHalfSpan_ = static_cast<float>(nominalLatencySamples_);
    delayA_ = static_cast<float>(nominalLatencySamples_);
    delayB_ = static_cast<float>(nominalLatencySamples_);
    currentPeriodSamples_ = static_cast<float>(sampleRate_ / 200.0);

    lastTargetMidi_ = -1;
    sustainedNoteSec_ = 0.0f;
    prevDetectedMidi_ = 0.0f;
    slowPitchTrackerMidi_ = 0.0f;
    currentRatio_ = 1.0f;
    targetRatio_ = 1.0f;

    mixSmoother_.reset(params_.mix);
    enableSmoother_.reset(params_.enabled ? 1.0f : 0.0f);
    telemetry_ = PitchTelemetry{};
}

float PitchCorrector::readInterpolated(const std::array<float, kBufSize>& buf, float fractionalPos) const noexcept
{
    // Wrap fractionalPos into [0, kBufSize)
    float pos = fractionalPos;
    while (pos < 0.0f) pos += static_cast<float>(kBufSize);
    while (pos >= static_cast<float>(kBufSize)) pos -= static_cast<float>(kBufSize);

    const int i1 = static_cast<int>(pos);
    const float frac = pos - static_cast<float>(i1);

    const int i0 = (i1 - 1) & kBufMask;
    const int i2 = (i1 + 1) & kBufMask;
    const int i3 = (i1 + 2) & kBufMask;

    const float y0 = buf[static_cast<std::size_t>(i0)];
    const float y1 = buf[static_cast<std::size_t>(i1)];
    const float y2 = buf[static_cast<std::size_t>(i2)];
    const float y3 = buf[static_cast<std::size_t>(i3)];

    // 4-point cubic Hermite (Catmull-Rom) interpolation for transparent high-frequency retention
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PitchCorrector::updateTargetRatio(const PitchEstimate& pitch, int numSamples) noexcept
{
    const float blockSec = static_cast<float>(numSamples) / static_cast<float>(sampleRate_);

    if (!pitch.isVoiced || pitch.frequencyHz < 55.0f || pitch.frequencyHz > 1100.0f)
    {
        // Unvoiced consonants & breaths: smoothly relax pitch ratio back to 1.0 so sibilants stay pristine
        targetRatio_ = 1.0f;
        sustainedNoteSec_ = 0.0f;
        telemetry_.isVoiced = false;
        telemetry_.detectedHz = 0.0f;
        telemetry_.targetHz = 0.0f;
        telemetry_.detectedMidiNote = -1;
        telemetry_.targetMidiNote = -1;
        telemetry_.inputCentsOffset = 0.0f;
        telemetry_.appliedCorrectionCents = 1200.0f * std::log2(std::max(0.5f, currentRatio_));
        return;
    }

    // Track vocal pitch period smoothly for pitch-synchronous crossfading
    if (pitch.periodSamples >= 24.0f && pitch.periodSamples <= 1500.0f)
        currentPeriodSamples_ += 0.35f * (pitch.periodSamples - currentPeriodSamples_);

    const float detectedMidi = hzToMidi(pitch.frequencyHz);
    const int nearestChromatic = static_cast<int>(std::round(detectedMidi));
    const float chromaticCents = (detectedMidi - static_cast<float>(nearestChromatic)) * 100.0f;

    const int targetMidi = quantizeMidiNoteToScale(detectedMidi,
                                                   params_.rootNote,
                                                   params_.scaleType,
                                                   lastTargetMidi_,
                                                   16.0f);

    const bool noteChanged = (lastTargetMidi_ >= 0 && targetMidi != lastTargetMidi_);
    if (noteChanged)
    {
        sustainedNoteSec_ = 0.0f;
        slowPitchTrackerMidi_ = detectedMidi;
    }
    else
    {
        sustainedNoteSec_ += blockSec;
        if (lastTargetMidi_ < 0)
            slowPitchTrackerMidi_ = detectedMidi;
        else
        {
            // Slow tracker (~180 ms time constant) isolates mean pitch from 5-7 Hz vibrato modulation
            const float slowAlpha = 1.0f - std::exp(-blockSec / 0.18f);
            slowPitchTrackerMidi_ += slowAlpha * (detectedMidi - slowPitchTrackerMidi_);
        }
    }
    lastTargetMidi_ = targetMidi;

    // Vibrato deviation = fast pitch minus slow mean pitch (clamped to +/- 65 cents)
    const float vibratoDevSemitones = clampf(detectedMidi - slowPitchTrackerMidi_, -0.65f, 0.65f);

    // HUMANIZE: on sustained notes (> 90ms), re-inject natural vibrato deviation proportional to humanize
    const float sustainRamp = clampf((sustainedNoteSec_ - 0.09f) / 0.16f, 0.0f, 1.0f);
    const float preservedVibrato = vibratoDevSemitones * params_.humanize * sustainRamp;

    // FLEX / TRANSITION: detect rapid vocal pitch glides (semitones/sec) and soften correction during transitions
    float glideRateStPerSec = 0.0f;
    if (prevDetectedMidi_ > 1.0f && blockSec > 1.0e-5f)
        glideRateStPerSec = std::abs(detectedMidi - prevDetectedMidi_) / blockSec;
    prevDetectedMidi_ = detectedMidi;

    const float glideSoftener = clampf(glideRateStPerSec / 28.0f, 0.0f, 1.0f) * params_.flexTransition * 0.75f;

    // Desired corrected MIDI pitch
    const float idealMidi = static_cast<float>(targetMidi) + preservedVibrato;
    const float correctedMidi = idealMidi + (detectedMidi - idealMidi) * glideSoftener;

    const float rawRatio = std::pow(2.0f, (correctedMidi - detectedMidi) / 12.0f);
    targetRatio_ = clampf(rawRatio, 0.75f, 1.35f); // Safe vocal correction range (~ +/- 5 semitones)

    telemetry_.isVoiced = true;
    telemetry_.detectedHz = pitch.frequencyHz;
    telemetry_.targetHz = midiToHz(static_cast<float>(targetMidi));
    telemetry_.detectedMidiNote = nearestChromatic;
    telemetry_.targetMidiNote = targetMidi;
    telemetry_.inputCentsOffset = chromaticCents;
    telemetry_.appliedCorrectionCents = 1200.0f * std::log2(std::max(0.5f, currentRatio_));
}

void PitchCorrector::processBlock(float* left,
                                  float* right,
                                  int numSamples,
                                  const PitchEstimate& pitch) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    mixSmoother_.setTarget(clampf(params_.mix, 0.0f, 1.0f));
    enableSmoother_.setTarget(params_.enabled ? 1.0f : 0.0f);

    updateTargetRatio(pitch, numSamples);

    // Effective retune slew time (ms): RetuneSpeed + Humanize sustain relaxation + Flex transition smoothing
    const float baseRetuneMs = clampf(params_.retuneSpeedMs, 0.5f, 220.0f);
    const float humanizeExtraMs = (sustainedNoteSec_ > 0.12f) ? (params_.humanize * 45.0f) : 0.0f;
    const float flexExtraMs = params_.flexTransition * 24.0f;
    const float effectiveMs = (pitch.isVoiced)
                                  ? std::max(0.5f, baseRetuneMs + humanizeExtraMs + flexExtraMs)
                                  : 22.0f; // Smooth release to 1.0 on unvoiced consonants

    const float ratioCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate_) * effectiveMs * 0.001f));
    const float minDelay = 4.0f;
    const float maxSafeDelay = static_cast<float>(kBufSize - 64);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = (right != nullptr) ? right[i] : inL;

        // Write input into circular buffer
        delayBufL_[static_cast<std::size_t>(writeIdx_)] = inL;
        if (right != nullptr)
            delayBufR_[static_cast<std::size_t>(writeIdx_)] = inR;

        // Latency-aligned dry signal (matches nominalLatencySamples_ for comb-free parallel Mix & Bypass)
        const int dryReadIdx = (writeIdx_ - nominalLatencySamples_) & kBufMask;
        const float dryL = delayBufL_[static_cast<std::size_t>(dryReadIdx)];
        const float dryR = (right != nullptr) ? delayBufR_[static_cast<std::size_t>(dryReadIdx)] : dryL;

        // Smooth pitch ratio sample-by-sample (prevents abrupt pitch steps)
        const float desiredRatio = params_.enabled ? targetRatio_ : 1.0f;
        currentRatio_ = desiredRatio + ratioCoeff * (currentRatio_ - desiredRatio);
        currentRatio_ = clampf(currentRatio_, 0.70f, 1.42f);

        const float enableAmt = enableSmoother_.process();
        const float mixAmt = mixSmoother_.process();
        const float totalWet = enableAmt * mixAmt;

        if (totalWet < 1.0e-5f)
        {
            // Keep shifter state gently centered when bypassed
            delayA_ = static_cast<float>(nominalLatencySamples_);
            delayB_ = static_cast<float>(nominalLatencySamples_);
            phase_ = 0.0f;
            left[i] = dryL;
            if (right != nullptr) right[i] = dryR;
            writeIdx_ = (writeIdx_ + 1) & kBufMask;
            continue;
        }

        // Advance both pitch-synchronous read heads at exact pitch ratio
        const float deltaDelay = 1.0f - currentRatio_;
        delayA_ += deltaDelay;
        delayB_ += deltaDelay;

        // Advance grain crossfade phase proportionally to |1 - ratio| / (2 * halfSpan)
        const float absDelta = std::abs(deltaDelay);
        const float prevPhase = phase_;
        const float prevPhaseB = (prevPhase >= 0.5f) ? (prevPhase - 0.5f) : (prevPhase + 0.5f);

        if (absDelta > 1.0e-5f)
        {
            const float dPhi = absDelta / std::max(64.0f, 2.0f * latchedHalfSpan_);
            phase_ += dPhi;
            if (phase_ >= 1.0f)
                phase_ -= std::floor(phase_);
        }
        else
        {
            // When pitch ratio is effectively 1.0, gently drift delays toward nominal latency
            delayA_ += 0.002f * (static_cast<float>(nominalLatencySamples_) - delayA_);
            delayB_ += 0.002f * (static_cast<float>(nominalLatencySamples_) - delayB_);
        }

        const float phaseA = phase_;
        const float phaseB = (phaseA >= 0.5f) ? (phaseA - 0.5f) : (phaseA + 0.5f);

        // Compute pitch-synchronous halfSpan = m * T0 (exact integer multiple of vocal period T0!)
        auto computeSyncHalfSpan = [&]() -> float
        {
            const float T0 = clampf(currentPeriodSamples_, 32.0f, 1100.0f);
            const float m = std::max(1.0f, std::round(static_cast<float>(nominalLatencySamples_) / T0));
            return clampf(m * T0, 64.0f, static_cast<float>(kBufSize / 3));
        };

        // Reset Head A at exact zero-weight crossing (phaseA wraps 1 -> 0)
        if (phaseA < prevPhase)
        {
            latchedHalfSpan_ = computeSyncHalfSpan();
            if (currentRatio_ >= 1.0f)
                delayA_ = clampf(delayB_ + latchedHalfSpan_, minDelay + 16.0f, maxSafeDelay);
            else
                delayA_ = clampf(delayB_ - latchedHalfSpan_, minDelay, maxSafeDelay);
        }

        // Reset Head B at exact zero-weight crossing (phaseB wraps 1 -> 0)
        if (phaseB < prevPhaseB)
        {
            latchedHalfSpan_ = computeSyncHalfSpan();
            if (currentRatio_ >= 1.0f)
                delayB_ = clampf(delayA_ + latchedHalfSpan_, minDelay + 16.0f, maxSafeDelay);
            else
                delayB_ = clampf(delayA_ - latchedHalfSpan_, minDelay, maxSafeDelay);
        }

        // Safety clamps in case of extreme sudden ratio reversals
        delayA_ = clampf(delayA_, minDelay, maxSafeDelay);
        delayB_ = clampf(delayB_, minDelay, maxSafeDelay);

        // Complementary Hann crossfade weights: wA + wB == 1.0 identically!
        const float sA = std::sin(kPi * phaseA);
        const float wA = sA * sA;
        const float wB = 1.0f - wA;

        const float posA = static_cast<float>(writeIdx_) - delayA_;
        const float posB = static_cast<float>(writeIdx_) - delayB_;

        const float wetL = wA * readInterpolated(delayBufL_, posA) + wB * readInterpolated(delayBufL_, posB);
        left[i] = dryL + totalWet * (wetL - dryL);

        if (right != nullptr)
        {
            const float wetR = wA * readInterpolated(delayBufR_, posA) + wB * readInterpolated(delayBufR_, posB);
            right[i] = dryR + totalWet * (wetR - dryR);
        }

        writeIdx_ = (writeIdx_ + 1) & kBufMask;
    }

    if (telemetry_.isVoiced)
        telemetry_.appliedCorrectionCents = 1200.0f * std::log2(std::max(0.5f, currentRatio_));
}

} // namespace alr::dsp
