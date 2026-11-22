#include "PitchPanel.h"
#include <algorithm>
#include <cmath>

namespace alr::ui
{

PitchPanel::PitchPanel(juce::AudioProcessorValueTreeState& apvts)
{
    enableBtn_.setClickingTogglesState(true);
    enableBtn_.setColour(juce::TextButton::buttonColourId, Palette::controlWell);
    enableBtn_.setColour(juce::TextButton::buttonOnColourId, Palette::accentBlue);
    enableBtn_.setColour(juce::TextButton::textColourOffId, Palette::textSecondary);
    enableBtn_.setColour(juce::TextButton::textColourOnId, Palette::accentBlue);
    enableBtn_.onStateChange = [this]()
    {
        enableBtn_.setButtonText(enableBtn_.getToggleState() ? "ACTIVE" : "OFF");
    };
    addAndMakeVisible(enableBtn_);

    rootBox_.addItemList(alr::ParameterManager::getRootNoteChoices(), 1);
    scaleBox_.addItemList(alr::ParameterManager::getScaleChoices(), 1);
    rangeBox_.addItemList(alr::ParameterManager::getPitchRangeChoices(), 1);
    addAndMakeVisible(rootBox_);
    addAndMakeVisible(scaleBox_);
    addAndMakeVisible(rangeBox_);

    enableAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, alr::params::pitchEnabled, enableBtn_);
    rootAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, alr::params::pitchRoot, rootBox_);
    scaleAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, alr::params::pitchScale, scaleBox_);
    rangeAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, alr::params::pitchRange, rangeBox_);

    retuneKnob_   = std::make_unique<RotaryKnobComponent>(apvts, alr::params::retuneSpeed, "RETUNE",   Palette::accentBlue, true);
    humanizeKnob_ = std::make_unique<RotaryKnobComponent>(apvts, alr::params::humanize,    "HUMANIZE", Palette::accentBlue, true);
    flexKnob_     = std::make_unique<RotaryKnobComponent>(apvts, alr::params::flex,        "FLEX",     Palette::accentBlue, true);
    mixKnob_      = std::make_unique<RotaryKnobComponent>(apvts, alr::params::pitchMix,    "MIX",      Palette::accentBlue, true);

    addAndMakeVisible(*retuneKnob_);
    addAndMakeVisible(*humanizeKnob_);
    addAndMakeVisible(*flexKnob_);
    addAndMakeVisible(*mixKnob_);
}

juce::String PitchPanel::midiNoteToName(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return "--";
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc = midiNote % 12;
    const int oct = (midiNote / 12) - 1;
    return juce::String(names[pc]) + juce::String(oct);
}

void PitchPanel::updateTelemetry(const alr::dsp::PitchTelemetry& telemetry)
{
    telemetry_ = telemetry;
    const float target = telemetry_.isVoiced ? std::clamp(telemetry_.appliedCorrectionCents, -100.0f, 100.0f) : 0.0f;
    smoothedCents_ += 0.30f * (target - smoothedCents_);
}

void PitchPanel::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    auto header = area.removeFromTop(24);
    enableBtn_.setBounds(header.removeFromRight(72).reduced(0, 1));

    area.removeFromTop(6);

    // Left selectors (Key, Scale, Vocal Range)
    auto leftSelectors = area.removeFromLeft(175);
    auto topSel = leftSelectors.removeFromTop(28);
    rootBox_.setBounds(topSel.removeFromLeft(66));
    topSel.removeFromLeft(6);
    scaleBox_.setBounds(topSel);

    leftSelectors.removeFromTop(8);
    rangeBox_.setBounds(leftSelectors.removeFromTop(26));

    area.removeFromLeft(14);

    // Right 4 compact knobs
    const int knobBlockW = 288;
    auto knobArea = area.removeFromRight(knobBlockW);
    const int kw = knobBlockW / 4;
    retuneKnob_->setBounds(knobArea.removeFromLeft(kw).reduced(2, 0));
    humanizeKnob_->setBounds(knobArea.removeFromLeft(kw).reduced(2, 0));
    flexKnob_->setBounds(knobArea.removeFromLeft(kw).reduced(2, 0));
    mixKnob_->setBounds(knobArea.reduced(2, 0));

    area.removeFromRight(14);
    meterBounds_ = area.reduced(0, 4);
}

void PitchPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(enableBtn_.getToggleState() ? Palette::accentBlue.withAlpha(0.45f) : Palette::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    g.setFont(ALRLookAndFeel::makeFont(10.5f, true));
    g.setColour(Palette::accentBlue);
    g.drawText("ALR PITCH", 14, 10, 140, 18, juce::Justification::centredLeft);

    g.setFont(ALRLookAndFeel::makeFont(9.0f, false));
    g.setColour(Palette::textMuted);
    g.drawText("REAL-TIME SCALE CORRECTION", 92, 11, 200, 16, juce::Justification::centredLeft);

    // Live Pitch Detection & Correction Visualizer
    if (!meterBounds_.isEmpty())
    {
        const auto mb = meterBounds_.toFloat();
        g.setColour(Palette::controlWell);
        g.fillRoundedRectangle(mb, 5.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(mb, 5.0f, 1.0f);

        auto inner = mb.reduced(10.0f, 7.0f);
        auto topRow = inner.removeFromTop(26.0f);

        const juce::String noteStr = telemetry_.isVoiced ? midiNoteToName(telemetry_.targetMidiNote) : "--";
        const juce::String hzStr   = telemetry_.isVoiced ? (juce::String(telemetry_.detectedHz, 1) + " Hz") : "NO PITCH";
        const juce::String corrStr = telemetry_.isVoiced
                                         ? ((smoothedCents_ >= 0.0f ? "+" : "") + juce::String(static_cast<int>(std::round(smoothedCents_))) + "c")
                                         : "0c";

        g.setFont(ALRLookAndFeel::makeFont(18.0f, true));
        g.setColour(telemetry_.isVoiced ? Palette::textPrimary : Palette::textMuted);
        g.drawText(noteStr, topRow.removeFromLeft(46.0f).toNearestInt(), juce::Justification::centredLeft);

        g.setFont(ALRLookAndFeel::makeFont(10.0f, false));
        g.setColour(Palette::textSecondary);
        g.drawText(hzStr, topRow.removeFromLeft(74.0f).toNearestInt(), juce::Justification::centredLeft);

        g.setFont(ALRLookAndFeel::makeFont(10.5f, true));
        g.setColour(Palette::accentBlue);
        g.drawText(corrStr, topRow.toNearestInt(), juce::Justification::centredRight);

        // Center-zero horizontal correction bar (-100c .. +100c)
        inner.removeFromTop(8.0f);
        auto bar = inner.withHeight(8.0f);
        g.setColour(Palette::panelElevated);
        g.fillRoundedRectangle(bar, 4.0f);

        const float centerX = bar.getCentreX();
        const float norm = std::clamp(smoothedCents_ / 100.0f, -1.0f, 1.0f);
        const float halfW = bar.getWidth() * 0.5f;
        const float fillW = std::abs(norm) * halfW;

        if (fillW > 1.0f)
        {
            const float x0 = (norm >= 0.0f) ? centerX : (centerX - fillW);
            g.setColour(Palette::accentBlue);
            g.fillRoundedRectangle(x0, bar.getY(), fillW, bar.getHeight(), 3.0f);
        }

        // Center tick
        g.setColour(Palette::textPrimary.withAlpha(0.75f));
        g.fillRect(centerX - 0.75f, bar.getY() - 2.0f, 1.5f, bar.getHeight() + 4.0f);
    }
}

} // namespace alr::ui
