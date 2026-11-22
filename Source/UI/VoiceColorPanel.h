#pragma once

#include "../Parameters/ParameterIDs.h"
#include "ALRLookAndFeel.h"
#include "RotaryKnobComponent.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class VoiceColorPanel : public juce::Component
{
public:
    explicit VoiceColorPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    std::unique_ptr<RotaryKnobComponent> bodyKnob_;
    std::unique_ptr<RotaryKnobComponent> warmthKnob_;
    std::unique_ptr<RotaryKnobComponent> clarityKnob_;
    std::unique_ptr<RotaryKnobComponent> presenceKnob_;
    std::unique_ptr<RotaryKnobComponent> airKnob_;
    std::unique_ptr<RotaryKnobComponent> densityKnob_;
    std::unique_ptr<RotaryKnobComponent> smoothKnob_;
    std::unique_ptr<RotaryKnobComponent> colorKnob_;
};

} // namespace alr::ui
