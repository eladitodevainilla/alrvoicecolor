#include "../Source/DSP/VocalProcessingChain.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace alr::dsp;

static int gPassed = 0;
static int gFailed = 0;

static void check(bool condition, const std::string& name, const std::string& details = "")
{
    if (condition)
    {
        ++gPassed;
        std::cout << "[PASS] " << name;
        if (!details.empty()) std::cout << "  (" << details << ")";
        std::cout << "\n";
    }
    else
    {
        ++gFailed;
        std::cout << "[FAIL] " << name;
        if (!details.empty()) std::cout << "  (" << details << ")";
        std::cout << "\n";
    }
}

// Synthesizes a rich harmonic vocal-like tone (fundamental + formant-weighted harmonics)
static std::vector<float> synthVocalTone(double sr, float durationSec, float f0Hz,
                                         float amp = 0.25f, float brightness = 1.0f,
                                         float resonanceHz = 0.0f, float resonanceGain = 0.0f,
                                         float vibratoRateHz = 0.0f, float vibratoCents = 0.0f)
{
    const int n = static_cast<int>(std::round(sr * durationSec));
    std::vector<float> out(static_cast<std::size_t>(n), 0.0f);
    double phase = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        double instF0 = f0Hz;
        if (vibratoRateHz > 0.0f && vibratoCents > 0.0f)
        {
            const double cents = static_cast<double>(vibratoCents) * std::sin(2.0 * 3.14159265358979323846 * vibratoRateHz * t);
            instF0 = f0Hz * std::pow(2.0, cents / 1200.0);
        }

        phase += 2.0 * 3.14159265358979323846 * instF0 / sr;
        double s = 0.0;
        for (int h = 1; h <= 64; ++h)
        {
            const double hf = instF0 * h;
            if (hf >= sr * 0.45) break;
            // Natural vocal rolloff shaped by brightness
            double w = std::pow(1.0 / static_cast<double>(h), 1.35 / std::max(0.35, static_cast<double>(brightness)));
            if (resonanceHz > 50.0f && std::abs(hf - resonanceHz) < resonanceHz * 0.14)
                w *= (1.0 + resonanceGain);
            s += w * std::sin(phase * h);
        }
        out[static_cast<std::size_t>(i)] = static_cast<float>(s * amp * 0.45);
    }
    return out;
}

static float measureRms(const std::vector<float>& buf, std::size_t startIdx = 0)
{
    double sum = 0.0;
    const std::size_t count = (buf.size() > startIdx) ? (buf.size() - startIdx) : 1;
    for (std::size_t i = startIdx; i < buf.size(); ++i)
        sum += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    return std::sqrt(static_cast<float>(sum / static_cast<double>(count)));
}

static float measureBandEnergyDb(const std::vector<float>& buf, double sr, float fLo, float fHi)
{
    constexpr std::size_t N = 2048;
    if (buf.size() < N) return -120.0f;
    Radix2FFT fft;
    fft.prepare(N);
    std::vector<float> mags(N / 2 + 1, 0.0f);
    fft.computeMagnitudeSpectrum(buf.data() + (buf.size() - N), mags.data());

    const std::size_t b0 = static_cast<std::size_t>(clampf(fLo * N / static_cast<float>(sr), 1.0f, static_cast<float>(N / 2)));
    const std::size_t b1 = static_cast<std::size_t>(clampf(fHi * N / static_cast<float>(sr), static_cast<float>(b0), static_cast<float>(N / 2)));
    double p = 0.0;
    for (std::size_t k = b0; k <= b1; ++k)
        p += static_cast<double>(mags[k]) * static_cast<double>(mags[k]);
    return static_cast<float>(10.0 * std::log10(std::max(p / static_cast<double>(b1 - b0 + 1), 1.0e-12)));
}

static void testPitchDetection()
{
    std::cout << "\n--- 1. Pitch Detection (YIN + Octave Guard) ---\n";
    const double sr = 48000.0;
    PitchDetector det;
    det.prepare(sr);
    det.setPitchRange(VocalPitchRange::Full);

    // Test across Bass (98 Hz G2), Baritone (146.83 Hz D3), Tenor (220 Hz A3), Alto (329.63 Hz E4), Soprano (523.25 Hz C5)
    const std::vector<float> testFreqs = { 98.0f, 146.83f, 220.0f, 329.63f, 523.25f };
    float worstCentsErr = 0.0f;
    bool allDetected = true;

    for (float f : testFreqs)
    {
        det.reset();
        auto sig = synthVocalTone(sr, 0.25f, f, 0.28f);
        PitchEstimate est{};
        for (std::size_t i = 0; i + 256 <= sig.size(); i += 256)
            est = det.processBlock(sig.data() + i, 256);

        const float centsErr = std::abs(1200.0f * std::log2(std::max(1.0f, est.frequencyHz) / f));
        worstCentsErr = std::max(worstCentsErr, centsErr);
        if (!est.isVoiced || centsErr > 4.0f)
            allDetected = false;
    }

    check(allDetected, "YIN detects male & female vocal range (98 Hz - 523 Hz) without octave errors",
          "worst error = " + std::to_string(worstCentsErr) + " cents");
}

static void testPitchCorrection()
{
    std::cout << "\n--- 2. Automatic Pitch Correction (ALR PITCH) ---\n";
    const double sr = 48000.0;

    // 2a. Verify Scale Quantization across Major & Minor + Enharmonics
    const int cMajFromCs = PitchCorrector::quantizeMidiNoteToScale(61.1f, RootNoteOption::C, ScaleType::Major, -1); // C#+10c -> D (62)
    const int aMinFromGs = PitchCorrector::quantizeMidiNoteToScale(68.2f, RootNoteOption::A, ScaleType::Minor, -1); // G#+20c -> A (69)
    const int fSharpEqGb1 = PitchCorrector::rootOptionToPitchClass(RootNoteOption::FSharp);
    const int fSharpEqGb2 = PitchCorrector::rootOptionToPitchClass(RootNoteOption::Gb);
    check(cMajFromCs == 62 && aMinFromGs == 69 && fSharpEqGb1 == fSharpEqGb2 && fSharpEqGb1 == 6,
          "Scale Quantizer maps out-of-scale notes accurately (Major, Minor, Enharmonics)");

    // 2b. Real DSP pitch shifting test: feed flat A3 (213.7 Hz, -50 cents off A3=220 Hz) in A Minor,
    // verify the output signal is genuinely pitch-shifted up to ~220.0 Hz!
    PitchDetector inDet, outDet;
    PitchCorrector corr;
    inDet.prepare(sr);
    outDet.prepare(sr);
    corr.prepare(sr);

    PitchCorrectorParams pp{};
    pp.enabled = true;
    pp.rootNote = RootNoteOption::A;
    pp.scaleType = ScaleType::Minor;
    pp.retuneSpeedMs = 4.0f; // Fast retune
    pp.humanize = 0.0f;
    pp.flexTransition = 0.0f;
    pp.mix = 1.0f;
    corr.setParameters(pp);

    // Input: 214.0 Hz (approx -47.8 cents flat of A3=220.0 Hz; A3 is allowed in A Minor, G#3=207.65 is NOT in A Minor!)
    auto sig = synthVocalTone(sr, 0.70f, 214.0f, 0.30f);
    std::vector<float> outL = sig;
    std::vector<float> outR = sig;

    for (std::size_t i = 0; i + 256 <= sig.size(); i += 256)
    {
        const auto est = inDet.processBlock(sig.data() + i, 256);
        corr.processBlock(outL.data() + i, outR.data() + i, 256, est);
    }

    // Measure actual output pitch on the steady-state second half
    PitchEstimate outEst{};
    for (std::size_t i = sig.size() / 2; i + 256 <= outL.size(); i += 256)
        outEst = outDet.processBlock(outL.data() + i, 256);

    const float errFromA3Cents = std::abs(1200.0f * std::log2(std::max(1.0f, outEst.frequencyHz) / 220.0f));
    check(outEst.isVoiced && errFromA3Cents < 12.0f,
          "Real-time PSOLA pitch shifter shifts 214.0 Hz flat vocal up to A3 (220.0 Hz)",
          "measured output = " + std::to_string(outEst.frequencyHz) + " Hz, err = " + std::to_string(errFromA3Cents) + " cents");

    // 2c. Downward shift test: feed sharp C#4 (277.18 Hz, NOT in C Major) -> should pull to D4 (293.66 Hz) or C4 (261.63 Hz);
    // test 268.0 Hz (+41.6 cents sharp of C4=261.63 Hz) in C Major -> snaps DOWN to C4 (261.63 Hz)!
    inDet.reset();
    outDet.reset();
    corr.reset();
    pp.rootNote = RootNoteOption::C;
    pp.scaleType = ScaleType::Major;
    corr.setParameters(pp);

    auto sharpSig = synthVocalTone(sr, 0.70f, 268.0f, 0.30f);
    std::vector<float> sharpOut = sharpSig;
    for (std::size_t i = 0; i + 256 <= sharpSig.size(); i += 256)
    {
        const auto est = inDet.processBlock(sharpSig.data() + i, 256);
        corr.processBlock(sharpOut.data() + i, nullptr, 256, est);
    }
    PitchEstimate outEstDown{};
    for (std::size_t i = sharpOut.size() / 2; i + 256 <= sharpOut.size(); i += 256)
        outEstDown = outDet.processBlock(sharpOut.data() + i, 256);

    const float errFromC4Cents = std::abs(1200.0f * std::log2(std::max(1.0f, outEstDown.frequencyHz) / 261.6256f));
    check(outEstDown.isVoiced && errFromC4Cents < 12.0f,
          "Real-time PSOLA pitch shifter shifts 268.0 Hz sharp vocal down to C4 (261.63 Hz)",
          "measured output = " + std::to_string(outEstDown.frequencyHz) + " Hz, err = " + std::to_string(errFromC4Cents) + " cents");
}

static void testVADAndRelearn()
{
    std::cout << "\n--- 3. VAD & RELEARN (40s Active-Voice Engine) ---\n";
    const double sr = 48000.0;
    VoiceActivityDetector vad;
    vad.prepare(sr);

    // 3a. Silence test
    std::vector<float> silence(48000, 0.0f); // 1 second silence
    VADMetrics mSil{};
    for (std::size_t i = 0; i + 256 <= silence.size(); i += 256)
        mSil = vad.processBlock(silence.data() + i, 256);
    check(mSil.state == VoiceState::Silence && !mSil.isActiveVocal, "VAD classifies silence accurately");

    // 3b. Unvoiced sibilant/high-freq noise test
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> sibilance(24000, 0.0f);
    BiquadFilter hpSib;
    hpSib.setParameters(BiquadType::HighPass, sr, 5500.0f, 0.707f);
    for (float& s : sibilance) s = hpSib.processSample(dist(rng) * 0.15f);
    VADMetrics mSib{};
    for (std::size_t i = 0; i + 256 <= sibilance.size(); i += 256)
        mSib = vad.processBlock(sibilance.data() + i, 256, 0.12f);
    check(mSib.state == VoiceState::UnvoicedConsonant && !mSib.isActiveVocal,
          "VAD distinguishes unvoiced sibilant consonant from active voiced vocal");

    // 3c. Active voice test
    auto voiceSig = synthVocalTone(sr, 0.5f, 185.0f, 0.25f);
    VADMetrics mVox{};
    for (std::size_t i = 0; i + 256 <= voiceSig.size(); i += 256)
        mVox = vad.processBlock(voiceSig.data() + i, 256, 0.92f);
    check(mVox.state == VoiceState::ActiveVoice && mVox.isActiveVocal,
          "VAD detects active voiced vocal accurately");

    // 3d. RELEARN ignores silence: feed 6 seconds silence + 4 seconds active voice + 5 seconds silence
    // Active time accumulated must be ~4.0 seconds (NOT 15 seconds)!
    VocalProcessingChain chain;
    chain.prepare(sr, 512);
    chain.startRelearn();

    auto darkVoice = synthVocalTone(sr, 4.0f, 115.0f, 0.28f, 0.45f, 320.0f, 3.0f); // Low dark muddy baritone
    std::vector<float> sil6s(static_cast<std::size_t>(sr * 6.0), 0.0f);
    std::vector<float> sil5s(static_cast<std::size_t>(sr * 5.0), 0.0f);

    auto feedMono = [&](std::vector<float>& buf)
    {
        for (std::size_t i = 0; i + 512 <= buf.size(); i += 512)
            chain.processBlock(buf.data() + i, nullptr, 512);
    };

    feedMono(sil6s);
    const float secAfterFirstSilence = chain.getTelemetry().relearnActiveSec;
    feedMono(darkVoice);
    feedMono(sil5s);
    const float secAfterVoiceAndSilence = chain.getTelemetry().relearnActiveSec;

    chain.stopRelearn();
    const VoiceProfile darkProfile = chain.getVoiceProfile();

    check(secAfterFirstSilence < 0.05f && secAfterVoiceAndSilence > 3.5f && secAfterVoiceAndSilence <= 4.25f,
          "RELEARN ignores 11s of silence and counts only ~4.0s of active vocal",
          "after 6s silence = " + std::to_string(secAfterFirstSilence) + "s, total active = " + std::to_string(secAfterVoiceAndSilence) + "s");

    // 3e. Compare two contrasting voices: Dark/Muddy Baritone vs Bright/Resonant Soprano
    // Verify RELEARN adapts DIFFERENT corrective targets to each voice!
    chain.resetProfile();
    chain.startRelearn();
    auto brightVoice = synthVocalTone(sr, 4.0f, 340.0f, 0.22f, 2.2f, 3800.0f, 4.5f); // Bright soprano with 3.8 kHz resonance
    feedMono(brightVoice);
    chain.stopRelearn();
    const VoiceProfile brightProfile = chain.getVoiceProfile();

    const bool distinctProfiles =
        darkProfile.isValid && brightProfile.isValid &&
        (brightProfile.medianF0Hz > darkProfile.medianF0Hz + 150.0f) &&
        (brightProfile.targets.hpfCutoffHz > darkProfile.targets.hpfCutoffHz + 20.0f) &&
        (darkProfile.targets.correctiveAirDb > brightProfile.targets.correctiveAirDb);

    check(distinctProfiles,
          "RELEARN builds distinct voice-adapted profiles (Dark Baritone vs Bright Soprano)",
          "Dark F0=" + std::to_string(static_cast<int>(darkProfile.medianF0Hz)) + "Hz HPF=" + std::to_string(static_cast<int>(darkProfile.targets.hpfCutoffHz)) +
          "Hz | Bright F0=" + std::to_string(static_cast<int>(brightProfile.medianF0Hz)) + "Hz HPF=" + std::to_string(static_cast<int>(brightProfile.targets.hpfCutoffHz)) + "Hz");

    // 3f. Verify 40-second automatic completion cap
    chain.resetProfile();
    chain.startRelearn();
    auto chunk1s = synthVocalTone(sr, 1.0f, 200.0f, 0.25f);
    for (int sec = 0; sec < 45; ++sec)
    {
        auto copy = chunk1s;
        feedMono(copy);
    }
    const auto tel40 = chain.getTelemetry();
    check(!tel40.isRelearning && tel40.hasProfile && tel40.relearnActiveSec >= 39.9f && tel40.relearnActiveSec <= 40.05f,
          "RELEARN automatically completes and locks profile at 40.0s active vocal limit",
          "final active = " + std::to_string(tel40.relearnActiveSec) + "s");
}

static void testVoiceColorControlsAndSafety()
{
    std::cout << "\n--- 4. Voice Color Controls & Anti-Clipping Safety ---\n";
    const double sr = 48000.0;

    auto runWithParams = [&](const ChainParameters& p, const std::vector<float>& input)
    {
        VocalProcessingChain chain;
        chain.prepare(sr, 512);
        chain.setParameters(p);
        std::vector<float> outL = input;
        std::vector<float> outR = input;
        for (std::size_t i = 0; i + 512 <= outL.size(); i += 512)
            chain.processBlock(outL.data() + i, outR.data() + i, 512);
        return outL;
    };

    // Broadband vocal-like test signal (180 Hz fundamental + rich harmonics + noise floor for air)
    auto testSig = synthVocalTone(sr, 0.6f, 180.0f, 0.22f, 1.1f);
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-0.02f, 0.02f);
    for (float& s : testSig) s += dist(rng);

    ChainParameters neutral{};
    neutral.autoGainComp = false;
    neutral.body = 0.0f; neutral.warmth = 0.0f; neutral.presence = 0.0f;
    neutral.air = 0.0f; neutral.clarity = 0.0f; neutral.density = 0.0f;
    neutral.smooth = 0.0f; neutral.color = 1.0f;
    auto baseOut = runWithParams(neutral, testSig);

    // BODY raises 140-220 Hz energy
    auto pBody = neutral; pBody.body = 1.0f;
    auto bodyOut = runWithParams(pBody, testSig);
    check(measureBandEnergyDb(bodyOut, sr, 140.0f, 220.0f) > measureBandEnergyDb(baseOut, sr, 140.0f, 220.0f) + 1.5f,
          "BODY knob measurably enhances low-mid vocal weight (140-220 Hz)");

    // PRESENCE raises 3000-4500 Hz energy
    auto pPres = neutral; pPres.presence = 1.0f;
    auto presOut = runWithParams(pPres, testSig);
    check(measureBandEnergyDb(presOut, sr, 3000.0f, 4500.0f) > measureBandEnergyDb(baseOut, sr, 3000.0f, 4500.0f) + 1.5f,
          "PRESENCE knob measurably boosts vocal presence (3-4.5 kHz)");

    // AIR raises 10-16 kHz energy
    auto pAir = neutral; pAir.air = 1.0f;
    auto airOut = runWithParams(pAir, testSig);
    check(measureBandEnergyDb(airOut, sr, 10000.0f, 16000.0f) > measureBandEnergyDb(baseOut, sr, 10000.0f, 16000.0f) + 2.0f,
          "AIR knob measurably opens high-frequency sheen (10-16 kHz)");

    // CLARITY cuts muddy 340-440 Hz relative to 2.2 kHz articulation
    auto pClar = neutral; pClar.clarity = 1.0f;
    auto clarOut = runWithParams(pClar, testSig);
    const float baseRatio = measureBandEnergyDb(baseOut, sr, 1900.0f, 2600.0f) - measureBandEnergyDb(baseOut, sr, 320.0f, 440.0f);
    const float clarRatio = measureBandEnergyDb(clarOut, sr, 1900.0f, 2600.0f) - measureBandEnergyDb(clarOut, sr, 320.0f, 440.0f);
    check(clarRatio > baseRatio + 2.5f,
          "CLARITY knob removes low-mid mud and enhances vocal intelligibility");

    // WARMTH generates genuine 2nd harmonic distortion on a pure 250 Hz sine wave
    std::vector<float> pureSine(static_cast<std::size_t>(sr * 0.4), 0.0f);
    for (std::size_t i = 0; i < pureSine.size(); ++i)
        pureSine[i] = 0.35f * std::sin(kTwoPi * 250.0f * static_cast<float>(i) / static_cast<float>(sr));
    auto cleanSineOut = runWithParams(neutral, pureSine);
    auto pWarm = neutral; pWarm.warmth = 1.0f;
    auto warmSineOut = runWithParams(pWarm, pureSine);
    const float cleanH2 = measureBandEnergyDb(cleanSineOut, sr, 470.0f, 530.0f);
    const float warmH2 = measureBandEnergyDb(warmSineOut, sr, 470.0f, 530.0f);
    check(warmH2 > cleanH2 + 12.0f,
          "WARMTH knob (2x oversampled) generates authentic analog 2nd-order harmonics",
          "H2 rise = +" + std::to_string(warmH2 - cleanH2) + " dB");

    // SMOOTH dynamically attenuates a harsh 4.25 kHz protruding resonance
    auto harshSig = synthVocalTone(sr, 0.5f, 210.0f, 0.25f, 1.2f, 4250.0f, 6.0f);
    auto harshBase = runWithParams(neutral, harshSig);
    auto pSmooth = neutral; pSmooth.smooth = 1.0f;
    auto harshSmoothed = runWithParams(pSmooth, harshSig);
    const float harshCutDb = measureBandEnergyDb(harshBase, sr, 4000.0f, 4500.0f) - measureBandEnergyDb(harshSmoothed, sr, 4000.0f, 4500.0f);
    check(harshCutDb > 1.0f,
          "SMOOTH knob dynamically suppresses harsh protruding vocal resonances",
          "4.25 kHz attenuation = -" + std::to_string(harshCutDb) + " dB");

    // DENSITY compresses dynamic range on alternating soft/loud signal
    std::vector<float> dynSig(static_cast<std::size_t>(sr * 0.8), 0.0f);
    for (std::size_t i = 0; i < dynSig.size(); ++i)
    {
        const float amp = (i < dynSig.size() / 2) ? 0.08f : 0.65f;
        dynSig[i] = amp * std::sin(kTwoPi * 220.0f * static_cast<float>(i) / static_cast<float>(sr));
    }
    auto pDens = neutral; pDens.density = 1.0f;
    auto densOut = runWithParams(pDens, dynSig);
    const float inDynSpreadDb = gainToDb(measureRms(dynSig, dynSig.size() * 5 / 8)) - gainToDb(measureRms(std::vector<float>(dynSig.begin() + 4000, dynSig.begin() + dynSig.size() / 2)));
    const float outDynSpreadDb = gainToDb(measureRms(densOut, densOut.size() * 5 / 8)) - gainToDb(measureRms(std::vector<float>(densOut.begin() + 4000, densOut.begin() + densOut.size() / 2)));
    check(outDynSpreadDb < inDynSpreadDb - 2.0f,
          "DENSITY knob reduces vocal dynamic spread smoothly",
          "spread reduced from " + std::to_string(inDynSpreadDb) + " dB to " + std::to_string(outDynSpreadDb) + " dB");

    // EXTREME STRESS TEST: ALL knobs at 100% + Hot Input (+0 dBFS) + Pitch ON -> Zero NaN/Inf, No hard digital clipping (>1.0)
    ChainParameters extreme{};
    extreme.autoGainComp = false;
    extreme.body = 1.0f; extreme.warmth = 1.0f; extreme.presence = 1.0f;
    extreme.air = 1.0f; extreme.clarity = 1.0f; extreme.density = 1.0f;
    extreme.smooth = 1.0f; extreme.color = 1.0f;
    extreme.outputGainDb = 12.0f;
    extreme.pitch.enabled = true;
    extreme.pitch.retuneSpeedMs = 1.0f;
    extreme.pitch.mix = 1.0f;

    auto hotSig = synthVocalTone(sr, 0.5f, 230.0f, 0.95f);
    auto extOut = runWithParams(extreme, hotSig);
    float maxAbs = 0.0f;
    bool allFinite = true;
    for (float s : extOut)
    {
        if (!std::isfinite(s)) allFinite = false;
        maxAbs = std::max(maxAbs, std::abs(s));
    }
    check(allFinite && maxAbs <= 1.0f,
          "Extreme all-knobs-100% + +12dB output stress test stays finite and clip-free (<= 0 dBFS)",
          "maxAbs = " + std::to_string(maxAbs));
}

static void testMultiSampleRateAndCPU()
{
    std::cout << "\n--- 5. Multi-Sample-Rate Stability, Latency & CPU Benchmark ---\n";
    const std::vector<double> rates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool allStable = true;

    for (double sr : rates)
    {
        VocalProcessingChain chain;
        chain.prepare(sr, 512);
        ChainParameters p{};
        p.pitch.enabled = true;
        p.pitch.rootNote = RootNoteOption::D;
        p.pitch.scaleType = ScaleType::Minor;
        p.body = 0.5f; p.warmth = 0.4f; p.presence = 0.5f; p.air = 0.4f;
        p.clarity = 0.4f; p.density = 0.5f; p.smooth = 0.4f; p.color = 0.85f;
        chain.setParameters(p);

        auto sig = synthVocalTone(sr, 0.25f, 240.0f, 0.25f);
        std::vector<float> L = sig, R = sig;
        for (std::size_t i = 0; i + 512 <= L.size(); i += 512)
            chain.processBlock(L.data() + i, R.data() + i, 512);

        for (float s : L)
            if (!std::isfinite(s) || std::abs(s) > 1.0f) allStable = false;
        if (chain.getLatencySamples() <= 0) allStable = false;
    }
    check(allStable, "Stable across 44.1 / 48 / 88.2 / 96 / 192 kHz with valid latency reporting");

    // Real-time CPU benchmark at 48 kHz, 256-sample stereo buffers, full chain + ALR PITCH + RELEARN active!
    const double sr = 48000.0;
    const int blockSize = 256;
    const int totalSeconds = 10;
    const int numBlocks = static_cast<int>((sr * totalSeconds) / blockSize);

    VocalProcessingChain chain;
    chain.prepare(sr, blockSize);
    ChainParameters p{};
    p.pitch.enabled = true;
    p.pitch.rootNote = RootNoteOption::C;
    p.pitch.scaleType = ScaleType::Major;
    p.body = 0.6f; p.warmth = 0.5f; p.presence = 0.5f; p.air = 0.5f;
    p.clarity = 0.5f; p.density = 0.5f; p.smooth = 0.5f; p.color = 0.9f;
    chain.setParameters(p);

    auto blockAudio = synthVocalTone(sr, static_cast<float>(blockSize) / static_cast<float>(sr), 215.0f, 0.25f);
    std::vector<float> L(blockSize), R(blockSize);

    // Measure normal active processing (Full 10-stage chain + ALR PITCH ON)
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < numBlocks; ++b)
    {
        std::copy(blockAudio.begin(), blockAudio.end(), L.begin());
        std::copy(blockAudio.begin(), blockAudio.end(), R.begin());
        chain.processBlock(L.data(), R.data(), blockSize);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedSec = std::chrono::duration<double>(t1 - t0).count();
    const double cpuPercent = (elapsedSec / static_cast<double>(totalSeconds)) * 100.0;

    // Measure with RELEARN actively running simultaneously
    chain.startRelearn();
    const auto t2 = std::chrono::steady_clock::now();
    for (int b = 0; b < numBlocks; ++b)
    {
        std::copy(blockAudio.begin(), blockAudio.end(), L.begin());
        std::copy(blockAudio.begin(), blockAudio.end(), R.begin());
        chain.processBlock(L.data(), R.data(), blockSize);
    }
    const auto t3 = std::chrono::steady_clock::now();
    const double elapsedRelearnSec = std::chrono::duration<double>(t3 - t2).count();
    const double cpuRelearnPercent = (elapsedRelearnSec / static_cast<double>(totalSeconds)) * 100.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "Full Stereo Chain + ALR PITCH = " << cpuPercent << "% RT | With RELEARN active = "
        << cpuRelearnPercent << "% RT | Latency = " << chain.getLatencySamples()
        << " samples (" << (1000.0 * chain.getLatencySamples() / sr) << " ms)";

    check(cpuPercent < 18.0 && cpuRelearnPercent < 25.0,
          "CPU efficiency benchmark (48 kHz, 256-sample stereo, 10s audio)", oss.str());
}

int main()
{
    std::cout << "=========================================================\n";
    std::cout << " ALR VOICE COLOR — Real-Time DSP Verification Suite\n";
    std::cout << "=========================================================\n";

    testPitchDetection();
    testPitchCorrection();
    testVADAndRelearn();
    testVoiceColorControlsAndSafety();
    testMultiSampleRateAndCPU();

    std::cout << "\n=========================================================\n";
    std::cout << " SUMMARY: " << gPassed << " passed, " << gFailed << " failed\n";
    std::cout << "=========================================================\n";
    return (gFailed == 0) ? 0 : 1;
}
