#pragma once

#include "../DSP/PitchCorrector.h"
#include "../Parameters/ParameterIDs.h"
#include "../Parameters/ParameterManager.h"
#include "ALRLookAndFeel.h"
#include "RotaryKnobComponent.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class PitchPanel : public juce::Component
{
public:
    explicit PitchPanel(juce::AudioProcessorValueTreeState& apvts);

    void updateTelemetry(const alr::dsp::PitchTelemetry& telemetry);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    static juce::String midiNoteToName(int midiNote);

    juce::TextButton enableBtn_{ "OFF" };
    juce::ComboBox rootBox_;
    juce::ComboBox scaleBox_;
    juce::ComboBox rangeBox_;

    std::unique_ptr<RotaryKnobComponent> retuneKnob_;
    std::unique_ptr<RotaryKnobComponent> humanizeKnob_;
    std::unique_ptr<RotaryKnobComponent> flexKnob_;
    std::unique_ptr<RotaryKnobComponent> mixKnob_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rangeAtt_;

    juce::Rectangle<int> meterBounds_;
    alr::dsp::PitchTelemetry telemetry_{};
    float smoothedCents_ = 0.0f;
};

} // namespace alr::ui
