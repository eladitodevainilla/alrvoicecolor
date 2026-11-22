#include "VocalProcessingChain.h"
#include <algorithm>
#include <cmath>

namespace alr::dsp
{

void VocalProcessingChain::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    maxBlockSize_ = std::max(64, maxBlockSize);

    monoScratch_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    dryScratchL_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    dryScratchR_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);

    inputDcBlocker_.setParameters(BiquadType::HighPass, sampleRate_, 20.0f, 0.707f);

    vad_.prepare(sampleRate_);
    voiceAnalyzer_.prepare(sampleRate_);
    pitchDetector_.prepare(sampleRate_);
    pitchCorrector_.prepare(sampleRate_);
    vocalEQ_.prepare(sampleRate_);
    resonanceCtrl_.prepare(sampleRate_);
    saturation_.prepare(sampleRate_);
    dynamics_.prepare(sampleRate_);

    outputGainSmoother_.prepare(sampleRate_, 12.0f);
    globalMixSmoother_.prepare(sampleRate_, 12.0f);
    bypassWetSmoother_.prepare(sampleRate_, 10.0f);

    reset();
}

void VocalProcessingChain::reset() noexcept
{
    dryDelayL_.fill(0.0f);
    dryDelayR_.fill(0.0f);
    dryDelayWrite_ = 0;

    inputDcBlocker_.reset();
    vad_.reset();
    pitchDetector_.reset();
    pitchCorrector_.reset();
    vocalEQ_.reset();
    resonanceCtrl_.reset();
    saturation_.reset();
    dynamics_.reset();

    outputGainSmoother_.reset(dbToGain(params_.outputGainDb));
    globalMixSmoother_.reset(params_.globalMix);
    bypassWetSmoother_.reset(params_.bypass ? 0.0f : 1.0f);

    agcInRmsSq_ = 1.0e-6f;
    agcOutRmsSq_ = 1.0e-6f;
    agcGainSmoothed_ = 1.0f;

    preSpecFifo_.reset();
    postSpecFifo_.reset();
}

void VocalProcessingChain::setParameters(const ChainParameters& params) noexcept
{
    params_ = params;
}

void VocalProcessingChain::resetProfile() noexcept
{
    voiceAnalyzer_.clearProfile();
    VoiceProfile empty{};
    applyProfileToModules(empty);
}

void VocalProcessingChain::setVoiceProfile(const VoiceProfile& profile) noexcept
{
    voiceAnalyzer_.setProfile(profile);
    applyProfileToModules(voiceAnalyzer_.getProfile());
}

void VocalProcessingChain::applyProfileToModules(const VoiceProfile& profile) noexcept
{
    vocalEQ_.setVoiceProfile(profile);
    resonanceCtrl_.setVoiceProfile(profile);
    saturation_.setVoiceProfile(profile);
    dynamics_.setVoiceProfile(profile);
}

void VocalProcessingChain::processBlock(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    // Safeguard against hosts sending occasional oversized blocks without heap allocation
    int processed = 0;
    while (processed < numSamples)
    {
        const int n = std::min(numSamples - processed, maxBlockSize_);
        float* chL = left + processed;
        float* chR = (right != nullptr) ? (right + processed) : nullptr;

        // Check if RELEARN just completed and seamlessly propagate new profile to DSP modules
        if (voiceAnalyzer_.consumeProfileUpdatedFlag())
            applyProfileToModules(voiceAnalyzer_.getProfile());

        // -------------------------------------------------------------------
        // STAGE 1: INPUT STAGE (DC filter, input metering, aligned dry buffer)
        // -------------------------------------------------------------------
        float inPeak = 0.0f;
        double inSumSq = 0.0;
        const int latency = std::min(pitchCorrector_.getLatencySamples(), kDryDelayCap - 1);

        for (int i = 0; i < n; ++i)
        {
            // Sanitize NaN/Inf from rogue hosts
            float xL = std::isfinite(chL[i]) ? chL[i] : 0.0f;
            float xR = (chR != nullptr && std::isfinite(chR[i])) ? chR[i] : xL;

            xL = inputDcBlocker_.processLeft(xL);
            if (chR != nullptr) xR = inputDcBlocker_.processRight(xR);
            else xR = xL;

            chL[i] = xL;
            if (chR != nullptr) chR[i] = xR;

            const float mono = 0.5f * (xL + xR);
            monoScratch_[static_cast<std::size_t>(i)] = mono;

            inPeak = std::max(inPeak, std::max(std::abs(xL), std::abs(xR)));
            inSumSq += static_cast<double>(mono) * static_cast<double>(mono);

            // Delay dry signal to match PitchCorrector latency for phase-coherent Mix & Bypass
            dryDelayL_[static_cast<std::size_t>(dryDelayWrite_)] = xL;
            dryDelayR_[static_cast<std::size_t>(dryDelayWrite_)] = xR;
            const int readIdx = (dryDelayWrite_ - latency) & kDryDelayMask;
            dryScratchL_[static_cast<std::size_t>(i)] = dryDelayL_[static_cast<std::size_t>(readIdx)];
            dryScratchR_[static_cast<std::size_t>(i)] = dryDelayR_[static_cast<std::size_t>(readIdx)];
            dryDelayWrite_ = (dryDelayWrite_ + 1) & kDryDelayMask;
        }

        // Push pre-processed mono samples to GUI spectrum FIFO (lock-free, drops gracefully if UI slow)
        preSpecFifo_.push(monoScratch_.data(), static_cast<std::size_t>(n));

        const float inRms = std::sqrt(static_cast<float>(inSumSq / static_cast<double>(n)));

        // -------------------------------------------------------------------
        // STAGE 3 (Detection runs on clean input so pitch/VAD feed RELEARN cleanly)
        // -------------------------------------------------------------------
        pitchDetector_.setPitchRange(params_.pitch.pitchRange);
        const PitchEstimate pitchEst = pitchDetector_.processBlock(monoScratch_.data(), n);

        // -------------------------------------------------------------------
        // STAGE 2: VOICE ACTIVITY DETECTION + RELEARN INGESTION
        // -------------------------------------------------------------------
        const float pitchHint = (pitchEst.confidence > 0.0f) ? pitchEst.confidence : -1.0f;
        const VADMetrics vadMetrics = vad_.processBlock(monoScratch_.data(), n, pitchHint);

        if (voiceAnalyzer_.isLearning())
        {
            voiceAnalyzer_.ingestAudio(monoScratch_.data(), n, vadMetrics,
                                       pitchEst.frequencyHz, pitchEst.confidence);
        }

        // -------------------------------------------------------------------
        // STAGE 4: AUTOMATIC PITCH CORRECTION (ALR PITCH)
        // -------------------------------------------------------------------
        pitchCorrector_.setParameters(params_.pitch);
        pitchCorrector_.processBlock(chL, chR, n, pitchEst);

        // -------------------------------------------------------------------
        // STAGE 5: CORRECTIVE EQ (Adaptive HPF + VoiceProfile Corrective Curves)
        // -------------------------------------------------------------------
        VocalEQParams eqParams{};
        eqParams.body = params_.body;
        eqParams.clarity = params_.clarity;
        eqParams.presence = params_.presence;
        eqParams.air = params_.air;
        eqParams.color = params_.color;
        vocalEQ_.setParameters(eqParams);
        vocalEQ_.processCorrectiveBlock(chL, chR, n);

        // -------------------------------------------------------------------
        // STAGE 6: RESONANCE CONTROL (SMOOTH + Learned Resonances)
        // -------------------------------------------------------------------
        resonanceCtrl_.setParameters(params_.smooth, params_.color);
        resonanceCtrl_.processBlock(chL, chR, n);

        // -------------------------------------------------------------------
        // STAGE 7: HARMONIC SATURATION (WARMTH, 2x Oversampled)
        // -------------------------------------------------------------------
        saturation_.setParameters(params_.warmth, params_.color);
        saturation_.processBlock(chL, chR, n);

        // -------------------------------------------------------------------
        // STAGE 8: TONAL ENHANCEMENT (BODY, CLARITY, PRESENCE, AIR)
        // -------------------------------------------------------------------
        vocalEQ_.processTonalBlock(chL, chR, n);

        // -------------------------------------------------------------------
        // STAGE 9: DYNAMIC CONTROL (DENSITY + Soft Protective Limiter)
        // -------------------------------------------------------------------
        dynamics_.setParameters(params_.density, params_.color);
        dynamics_.processBlock(chL, chR, n);

        // -------------------------------------------------------------------
        // STAGE 10: OUTPUT STAGE (Auto-Gain Comp, Global Mix, Output Gain, Bypass)
        // -------------------------------------------------------------------
        double wetSumSq = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float m = (chR != nullptr) ? 0.5f * (chL[i] + chR[i]) : chL[i];
            wetSumSq += static_cast<double>(m) * static_cast<double>(m);
        }
        const float wetRms = std::sqrt(static_cast<float>(wetSumSq / static_cast<double>(n)));

        // Slow, transparent Auto-Gain Compensation active only during real vocal signal
        const float blockSec = static_cast<float>(n) / static_cast<float>(sampleRate_);
        if (params_.autoGainComp && vadMetrics.rmsDb > -45.0f)
        {
            const float agcAlpha = 1.0f - std::exp(-blockSec / 0.45f);
            agcInRmsSq_ += agcAlpha * (inRms * inRms - agcInRmsSq_);
            agcOutRmsSq_ += agcAlpha * (wetRms * wetRms - agcOutRmsSq_);
            const float targetAgc = clampf(std::sqrt((agcInRmsSq_ + 1.0e-7f) / (agcOutRmsSq_ + 1.0e-7f)),
                                           dbToGain(-6.0f), dbToGain(6.0f));
            agcGainSmoothed_ += agcAlpha * (targetAgc - agcGainSmoothed_);
        }
        else if (!params_.autoGainComp)
        {
            const float relAlpha = 1.0f - std::exp(-blockSec / 0.15f);
            agcGainSmoothed_ += relAlpha * (1.0f - agcGainSmoothed_);
        }

        outputGainSmoother_.setTarget(dbToGain(clampf(params_.outputGainDb, -18.0f, 18.0f)));
        globalMixSmoother_.setTarget(clampf(params_.globalMix, 0.0f, 1.0f));
        bypassWetSmoother_.setTarget(params_.bypass ? 0.0f : 1.0f);

        float outPeak = 0.0f;
        double outSumSq = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const float gOut = outputGainSmoother_.process();
            const float gMix = globalMixSmoother_.process();
            const float gActive = bypassWetSmoother_.process();

            const float dryL = dryScratchL_[static_cast<std::size_t>(i)];
            const float dryR = dryScratchR_[static_cast<std::size_t>(i)];

            const float procL = chL[i] * agcGainSmoothed_;
            const float mixedL = (dryL + gMix * (procL - dryL)) * gOut;
            float finalL = dryL + gActive * (mixedL - dryL);
            // Final safety soft-clip guard against extreme +18dB output gain clipping
            if (std::abs(finalL) > 0.98f)
                finalL = (finalL >= 0.0f ? 1.0f : -1.0f) * (0.98f + 0.019f * std::tanh((std::abs(finalL) - 0.98f) * 20.0f));
            chL[i] = finalL;

            float finalR = finalL;
            if (chR != nullptr)
            {
                const float procR = chR[i] * agcGainSmoothed_;
                const float mixedR = (dryR + gMix * (procR - dryR)) * gOut;
                finalR = dryR + gActive * (mixedR - dryR);
                if (std::abs(finalR) > 0.98f)
                    finalR = (finalR >= 0.0f ? 1.0f : -1.0f) * (0.98f + 0.019f * std::tanh((std::abs(finalR) - 0.98f) * 20.0f));
                chR[i] = finalR;
            }

            const float outMono = 0.5f * (finalL + finalR);
            monoScratch_[static_cast<std::size_t>(i)] = outMono;
            outPeak = std::max(outPeak, std::max(std::abs(finalL), std::abs(finalR)));
            outSumSq += static_cast<double>(outMono) * static_cast<double>(outMono);
        }

        // Push post-processed mono samples to GUI spectrum FIFO
        postSpecFifo_.push(monoScratch_.data(), static_cast<std::size_t>(n));

        const float outRms = std::sqrt(static_cast<float>(outSumSq / static_cast<double>(n)));

        // Update telemetry snapshot
        telemetry_.inputPeakDb = gainToDb(inPeak);
        telemetry_.inputRmsDb = gainToDb(inRms);
        telemetry_.outputPeakDb = gainToDb(outPeak);
        telemetry_.outputRmsDb = gainToDb(outRms);
        telemetry_.gainReductionDb = dynamics_.getGainReductionDb();
        telemetry_.resonanceCutDb = resonanceCtrl_.getCurrentMaxAttenuationDb();
        telemetry_.autoGainAppliedDb = gainToDb(agcGainSmoothed_);
        telemetry_.vad = vadMetrics;
        telemetry_.pitch = pitchCorrector_.getTelemetry();
        telemetry_.isRelearning = voiceAnalyzer_.isLearning();
        telemetry_.hasProfile = voiceAnalyzer_.hasCompletedProfile();
        telemetry_.relearnActiveSec = voiceAnalyzer_.getActiveSecondsAccumulated();
        telemetry_.relearnProgress = voiceAnalyzer_.getProgressNormalized();

        processed += n;
    }
}

} // namespace alr::dsp
