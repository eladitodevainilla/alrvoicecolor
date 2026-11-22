#include "RotaryKnobComponent.h"

namespace alr::ui
{

RotaryKnobComponent::RotaryKnobComponent(juce::AudioProcessorValueTreeState& apvts,
                                         const juce::String& paramID,
                                         const juce::String& labelText,
                                         juce::Colour accentColour,
                                         bool compact)
    : accent_(accentColour), compact_(compact)
{
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent_);
    slider_.setMouseDragSensitivity(compact_ ? 140 : 170);
    if (auto* param = apvts.getParameter(paramID))
        slider_.setDoubleClickReturnValue(true, static_cast<double>(param->convertFrom0to1(param->getDefaultValue())));

    addAndMakeVisible(slider_);

    titleLabel_.setText(labelText, juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setFont(ALRLookAndFeel::makeFont(compact_ ? 10.0f : 11.0f, true));
    titleLabel_.setColour(juce::Label::textColourId, Palette::textSecondary);
    addAndMakeVisible(titleLabel_);

    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setFont(ALRLookAndFeel::makeFont(compact_ ? 10.0f : 10.5f, false));
    valueLabel_.setColour(juce::Label::textColourId, Palette::textPrimary);
    addAndMakeVisible(valueLabel_);

    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider_);

    auto updateValText = [this, &apvts, paramID]()
    {
        if (auto* param = apvts.getParameter(paramID))
            valueLabel_.setText(param->getCurrentValueAsText(), juce::dontSendNotification);
    };
    slider_.onValueChange = updateValText;
    updateValText();
}

void RotaryKnobComponent::resized()
{
    auto area = getLocalBounds();
    const int labelH = compact_ ? 14 : 16;
    const int valH   = compact_ ? 13 : 15;

    titleLabel_.setBounds(area.removeFromTop(labelH));
    valueLabel_.setBounds(area.removeFromBottom(valH));
    slider_.setBounds(area.reduced(2));
}

void RotaryKnobComponent::paint(juce::Graphics&)
{
}

} // namespace alr::ui
