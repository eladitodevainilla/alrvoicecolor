#pragma once

#include "../PluginProcessor.h"
#include "ALRLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class RelearnPanel : public juce::Component
{
public:
    explicit RelearnPanel(ALRVoiceColorAudioProcessor& processor);

    void updateTelemetry(const alr::dsp::ChainTelemetry& telemetry, const alr::dsp::VoiceProfile& profile);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    ALRVoiceColorAudioProcessor& processor_;
    juce::TextButton relearnBtn_{ "RELEARN" };
    juce::TextButton resetBtn_{ "RESET PROFILE" };

    alr::dsp::ChainTelemetry telemetry_{};
    alr::dsp::VoiceProfile profile_{};
    float pulsePhase_ = 0.0f;
};

} // namespace alr::ui
