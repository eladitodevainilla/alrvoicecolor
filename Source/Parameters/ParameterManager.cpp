#include "ParameterManager.h"
#include <cmath>
#include <memory>

namespace alr
{

juce::StringArray ParameterManager::getRootNoteChoices()
{
    return {
        "C", "C#", "Db", "D", "D#", "Eb", "E",
        "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B"
    };
}

juce::StringArray ParameterManager::getScaleChoices()
{
    return { "Major", "Minor" };
}

juce::StringArray ParameterManager::getPitchRangeChoices()
{
    return { "Low (Bass/Baritone)", "Mid (Tenor/Alto)", "High (Alto/Soprano)", "Full Range" };
}

juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pctFormat = [](float value, int) { return juce::String(static_cast<int>(std::round(value * 100.0f))) + "%"; };
    auto dbFormat  = [](float value, int) { return (value >= 0.0f ? "+" : "") + juce::String(value, 1) + " dB"; };
    auto msFormat  = [](float value, int)
    {
        if (value < 1.5f) return juce::String("0 ms (Fast)");
        return juce::String(static_cast<int>(std::round(value))) + " ms";
    };

    // Global & Output
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ params::bypass, 1 }, "Bypass", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ params::autoGainComp, 1 }, "Auto Gain", true));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::outputGain, 1 }, "Output Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbFormat)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::globalMix, 1 }, "Global Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));

    // Voice Color Engine
    auto addPctParam = [&](const char* id, const char* name, float defaultVal)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), defaultVal,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctFormat)));
    };

    addPctParam(params::body,     "Body",     0.25f);
    addPctParam(params::warmth,   "Warmth",   0.20f);
    addPctParam(params::presence, "Presence", 0.25f);
    addPctParam(params::air,      "Air",      0.20f);
    addPctParam(params::clarity,  "Clarity",  0.25f);
    addPctParam(params::density,  "Density",  0.20f);
    addPctParam(params::smooth,   "Smooth",   0.25f);
    addPctParam(params::color,    "Color",    0.75f);

    // ALR Pitch Engine
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ params::pitchEnabled, 1 }, "ALR Pitch On", false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::pitchRoot, 1 }, "Root Note", getRootNoteChoices(), 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::pitchScale, 1 }, "Scale", getScaleChoices(), 0));

    juce::NormalisableRange<float> retuneRange(0.5f, 200.0f, 0.5f);
    retuneRange.setSkewForCentre(30.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::retuneSpeed, 1 }, "Retune Speed", retuneRange, 30.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(msFormat)));

    addPctParam(params::humanize, "Humanize", 0.35f);
    addPctParam(params::flex,     "Flex",     0.30f);

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::pitchRange, 1 }, "Pitch Range", getPitchRangeChoices(), 3));

    addPctParam(params::pitchMix, "Pitch Mix", 1.0f);

    return layout;
}

dsp::ChainParameters ParameterManager::readChainParameters(const juce::AudioProcessorValueTreeState& apvts) noexcept
{
    auto getRaw = [&](const char* id) noexcept -> float
    {
        if (auto* ptr = apvts.getRawParameterValue(id))
            return ptr->load(std::memory_order_relaxed);
        return 0.0f;
    };

    dsp::ChainParameters cp{};
    cp.bypass       = getRaw(params::bypass) > 0.5f;
    cp.autoGainComp = getRaw(params::autoGainComp) > 0.5f;
    cp.outputGainDb = getRaw(params::outputGain);
    cp.globalMix    = getRaw(params::globalMix);

    cp.body         = getRaw(params::body);
    cp.warmth       = getRaw(params::warmth);
    cp.presence     = getRaw(params::presence);
    cp.air          = getRaw(params::air);
    cp.clarity      = getRaw(params::clarity);
    cp.density      = getRaw(params::density);
    cp.smooth       = getRaw(params::smooth);
    cp.color        = getRaw(params::color);

    cp.pitch.enabled        = getRaw(params::pitchEnabled) > 0.5f;
    cp.pitch.rootNote       = static_cast<dsp::RootNoteOption>(static_cast<int>(std::round(getRaw(params::pitchRoot))));
    cp.pitch.scaleType      = static_cast<dsp::ScaleType>(static_cast<int>(std::round(getRaw(params::pitchScale))));
    cp.pitch.retuneSpeedMs  = getRaw(params::retuneSpeed);
    cp.pitch.humanize       = getRaw(params::humanize);
    cp.pitch.flexTransition = getRaw(params::flex);
    cp.pitch.pitchRange     = static_cast<dsp::VocalPitchRange>(static_cast<int>(std::round(getRaw(params::pitchRange))));
    cp.pitch.mix            = getRaw(params::pitchMix);

    return cp;
}

juce::ValueTree ParameterManager::serializeVoiceProfile(const dsp::VoiceProfile& profile)
{
    juce::ValueTree tree("ALRVoiceProfile");
    tree.setProperty("isValid", profile.isValid, nullptr);
    tree.setProperty("activeSec", profile.analyzedActiveSeconds, nullptr);
    tree.setProperty("validFrames", static_cast<int>(profile.validFrameCount), nullptr);
    tree.setProperty("medianF0", profile.medianF0Hz, nullptr);
    tree.setProperty("lowF0", profile.lowF0Hz, nullptr);
    tree.setProperty("highF0", profile.highF0Hz, nullptr);
    tree.setProperty("f1", profile.formant1Hz, nullptr);
    tree.setProperty("f2", profile.formant2Hz, nullptr);
    tree.setProperty("f3", profile.formant3Hz, nullptr);
    tree.setProperty("centroid", profile.spectralCentroidHz, nullptr);
    tree.setProperty("tilt", profile.spectralTiltDbPerOct, nullptr);
    tree.setProperty("harmonicDensity", profile.harmonicDensity, nullptr);
    tree.setProperty("dynRange", profile.dynamicRangeDb, nullptr);
    tree.setProperty("avgRms", profile.averageVocalRmsDb, nullptr);

    for (int i = 0; i < dsp::VoiceProfile::kNumBands; ++i)
        tree.setProperty(juce::Identifier("band_" + juce::String(i)), profile.bandRelDb[static_cast<std::size_t>(i)], nullptr);

    for (int i = 0; i < dsp::VoiceProfile::kMaxResonances; ++i)
    {
        tree.setProperty(juce::Identifier("resFreq_" + juce::String(i)), profile.resonanceFreqHz[static_cast<std::size_t>(i)], nullptr);
        tree.setProperty(juce::Identifier("resSev_" + juce::String(i)), profile.resonanceSeverityDb[static_cast<std::size_t>(i)], nullptr);
    }

    return tree;
}

dsp::VoiceProfile ParameterManager::deserializeVoiceProfile(const juce::ValueTree& tree)
{
    dsp::VoiceProfile p{};
    if (!tree.isValid() || !tree.hasType("ALRVoiceProfile"))
        return p;

    p.isValid               = static_cast<bool>(tree.getProperty("isValid", false));
    p.analyzedActiveSeconds = static_cast<float>(tree.getProperty("activeSec", 0.0f));
    p.validFrameCount       = static_cast<std::uint32_t>(static_cast<int>(tree.getProperty("validFrames", 0)));
    p.medianF0Hz            = static_cast<float>(tree.getProperty("medianF0", 165.0f));
    p.lowF0Hz               = static_cast<float>(tree.getProperty("lowF0", 110.0f));
    p.highF0Hz              = static_cast<float>(tree.getProperty("highF0", 260.0f));
    p.formant1Hz            = static_cast<float>(tree.getProperty("f1", 550.0f));
    p.formant2Hz            = static_cast<float>(tree.getProperty("f2", 1650.0f));
    p.formant3Hz            = static_cast<float>(tree.getProperty("f3", 2850.0f));
    p.spectralCentroidHz    = static_cast<float>(tree.getProperty("centroid", 1400.0f));
    p.spectralTiltDbPerOct  = static_cast<float>(tree.getProperty("tilt", -4.5f));
    p.harmonicDensity       = static_cast<float>(tree.getProperty("harmonicDensity", 0.65f));
    p.dynamicRangeDb        = static_cast<float>(tree.getProperty("dynRange", 11.0f));
    p.averageVocalRmsDb     = static_cast<float>(tree.getProperty("avgRms", -18.0f));

    for (int i = 0; i < dsp::VoiceProfile::kNumBands; ++i)
        p.bandRelDb[static_cast<std::size_t>(i)] = static_cast<float>(tree.getProperty(juce::Identifier("band_" + juce::String(i)), 0.0f));

    for (int i = 0; i < dsp::VoiceProfile::kMaxResonances; ++i)
    {
        p.resonanceFreqHz[static_cast<std::size_t>(i)]     = static_cast<float>(tree.getProperty(juce::Identifier("resFreq_" + juce::String(i)), 2600.0f));
        p.resonanceSeverityDb[static_cast<std::size_t>(i)] = static_cast<float>(tree.getProperty(juce::Identifier("resSev_" + juce::String(i)), 0.0f));
    }

    if (p.isValid)
        p.computeAdaptationTargets();

    return p;
}

} // namespace alr
