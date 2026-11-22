#include "VoiceColorPanel.h"

namespace alr::ui
{

VoiceColorPanel::VoiceColorPanel(juce::AudioProcessorValueTreeState& apvts)
{
    bodyKnob_     = std::make_unique<RotaryKnobComponent>(apvts, alr::params::body,     "BODY",     Palette::accentGold);
    warmthKnob_   = std::make_unique<RotaryKnobComponent>(apvts, alr::params::warmth,   "WARMTH",   Palette::accentGold);
    clarityKnob_  = std::make_unique<RotaryKnobComponent>(apvts, alr::params::clarity,  "CLARITY",  Palette::accentGold);
    presenceKnob_ = std::make_unique<RotaryKnobComponent>(apvts, alr::params::presence, "PRESENCE", Palette::accentGold);

    airKnob_      = std::make_unique<RotaryKnobComponent>(apvts, alr::params::air,      "AIR",      Palette::accentGold);
    densityKnob_  = std::make_unique<RotaryKnobComponent>(apvts, alr::params::density,  "DENSITY",  Palette::accentGold);
    smoothKnob_   = std::make_unique<RotaryKnobComponent>(apvts, alr::params::smooth,   "SMOOTH",   Palette::accentGold);
    colorKnob_    = std::make_unique<RotaryKnobComponent>(apvts, alr::params::color,    "COLOR",    Palette::textPrimary);

    addAndMakeVisible(*bodyKnob_);
    addAndMakeVisible(*warmthKnob_);
    addAndMakeVisible(*clarityKnob_);
    addAndMakeVisible(*presenceKnob_);
    addAndMakeVisible(*airKnob_);
    addAndMakeVisible(*densityKnob_);
    addAndMakeVisible(*smoothKnob_);
    addAndMakeVisible(*colorKnob_);
}

void VoiceColorPanel::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    area.removeFromTop(20); // Section header

    const int rowH = area.getHeight() / 2;
    auto topRow = area.removeFromTop(rowH);
    auto botRow = area;

    const int colW = topRow.getWidth() / 4;

    bodyKnob_->setBounds(topRow.removeFromLeft(colW).reduced(4, 2));
    warmthKnob_->setBounds(topRow.removeFromLeft(colW).reduced(4, 2));
    clarityKnob_->setBounds(topRow.removeFromLeft(colW).reduced(4, 2));
    presenceKnob_->setBounds(topRow.reduced(4, 2));

    airKnob_->setBounds(botRow.removeFromLeft(colW).reduced(4, 2));
    densityKnob_->setBounds(botRow.removeFromLeft(colW).reduced(4, 2));
    smoothKnob_->setBounds(botRow.removeFromLeft(colW).reduced(4, 2));
    colorKnob_->setBounds(botRow.reduced(4, 2));
}

void VoiceColorPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    g.setFont(ALRLookAndFeel::makeFont(10.5f, true));
    g.setColour(Palette::accentGold);
    g.drawText("VOICE COLOR", 14, 9, 160, 16, juce::Justification::centredLeft);

    g.setFont(ALRLookAndFeel::makeFont(9.5f, false));
    g.setColour(Palette::textMuted);
    g.drawText("ADAPTIVE TONAL & HARMONIC ENGINE", getWidth() - 230, 9, 216, 16, juce::Justification::centredRight);
}

} // namespace alr::ui
