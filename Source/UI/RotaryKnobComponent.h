#pragma once

#include "ALRLookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class RotaryKnobComponent : public juce::Component
{
public:
    RotaryKnobComponent(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& paramID,
                        const juce::String& labelText,
                        juce::Colour accentColour = Palette::accentGold,
                        bool compact = false);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Slider slider_;
    juce::Label titleLabel_;
    juce::Label valueLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::Colour accent_;
    bool compact_ = false;
};

} // namespace alr::ui
