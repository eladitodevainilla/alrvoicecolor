#pragma once

#include "../DSP/VocalProcessingChain.h"
#include "../DSP/VoiceProfile.h"
#include "ParameterIDs.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace alr
{
class ParameterManager
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Reads atomic parameter values directly from APVTS into a DSP ChainParameters struct (audio-thread safe)
    static dsp::ChainParameters readChainParameters(const juce::AudioProcessorValueTreeState& apvts) noexcept;

    // VoiceProfile <-> ValueTree serialization for DAW project persistence
    static juce::ValueTree serializeVoiceProfile(const dsp::VoiceProfile& profile);
    static dsp::VoiceProfile deserializeVoiceProfile(const juce::ValueTree& tree);

    static juce::StringArray getRootNoteChoices();
    static juce::StringArray getScaleChoices();
    static juce::StringArray getPitchRangeChoices();
};

} // namespace alr
