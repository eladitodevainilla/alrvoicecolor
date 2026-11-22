#pragma once

#include "PluginProcessor.h"
#include "UI/ALRLookAndFeel.h"
#include "UI/MeterComponent.h"
#include "UI/PitchPanel.h"
#include "UI/RelearnPanel.h"
#include "UI/RotaryKnobComponent.h"
#include "UI/SpectrumDisplay.h"
#include "UI/VoiceColorPanel.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ALRVoiceColorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit ALRVoiceColorAudioProcessorEditor(ALRVoiceColorAudioProcessor&);
    ~ALRVoiceColorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openSettingsMenu();
    void drawALRLogo(juce::Graphics& g, juce::Rectangle<float> area) const;

    ALRVoiceColorAudioProcessor& processor_;
    alr::ui::ALRLookAndFeel lookAndFeel_;

    // Header Controls
    juce::TextButton autoGainBtn_{ "AUTO GAIN" };
    juce::TextButton settingsBtn_{ "SETTINGS" };
    juce::TextButton bypassBtn_{ "BYPASS" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt_;

    // Core Panels
    alr::ui::RelearnPanel relearnPanel_;
    alr::ui::SpectrumDisplay spectrumDisplay_;
    alr::ui::VoiceColorPanel voiceColorPanel_;
    alr::ui::PitchPanel pitchPanel_;
    alr::ui::MeterComponent meterComponent_;

    // Output Section Knobs
    std::unique_ptr<alr::ui::RotaryKnobComponent> outputGainKnob_;
    std::unique_ptr<alr::ui::RotaryKnobComponent> globalMixKnob_;

    juce::Rectangle<int> outputPanelBounds_;
    alr::dsp::ChainTelemetry latestTelemetry_{};
    alr::dsp::VoiceProfile latestProfile_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ALRVoiceColorAudioProcessorEditor)
};
