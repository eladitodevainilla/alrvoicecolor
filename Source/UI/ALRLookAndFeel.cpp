#include "ALRLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace alr::ui
{

ALRLookAndFeel::ALRLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, Palette::bgMain);
    setColour(juce::ComboBox::backgroundColourId, Palette::controlWell);
    setColour(juce::ComboBox::outlineColourId, Palette::borderSubtle);
    setColour(juce::ComboBox::textColourId, Palette::textPrimary);
    setColour(juce::ComboBox::arrowColourId, Palette::textSecondary);
    setColour(juce::PopupMenu::backgroundColourId, Palette::panelElevated);
    setColour(juce::PopupMenu::textColourId, Palette::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accentGold.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::textPrimary);
    setColour(juce::Label::textColourId, Palette::textSecondary);
}

juce::Font ALRLookAndFeel::makeFont(float heightPx, bool bold)
{
    auto opts = juce::FontOptions(heightPx);
    if (bold)
        opts = opts.withStyle("Bold");
    return juce::Font(opts);
}

void ALRLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                      int x, int y, int width, int height,
                                      float sliderPos,
                                      float rotaryStartAngle,
                                      float rotaryEndAngle,
                                      juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(4.0f);
    const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
    const float radius   = diameter * 0.5f;
    const float cx       = bounds.getCentreX();
    const float cy       = bounds.getCentreY();
    const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const bool isHovered = slider.isMouseOverOrDragging();
    const bool isEnabled = slider.isEnabled();
    const auto accentCol = slider.findColour(juce::Slider::rotarySliderFillColourId).isOpaque()
                               ? slider.findColour(juce::Slider::rotarySliderFillColourId)
                               : Palette::accentGold;
    const auto activeAccent = isEnabled ? accentCol : Palette::textMuted;

    // 1. Track arc background
    const float arcRadius = radius - 2.0f;
    const float arcThick  = 2.6f;

    juce::Path trackArc;
    trackArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Palette::borderSubtle);
    g.strokePath(trackArc, juce::PathStrokeType(arcThick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Active value arc (supports bipolar center-zero if slider range spans negative..positive)
    const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
    const float zeroProp = isBipolar
                               ? static_cast<float>((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()))
                               : 0.0f;
    const float startAngle = rotaryStartAngle + zeroProp * (rotaryEndAngle - rotaryStartAngle);

    if (std::abs(angle - startAngle) > 0.01f)
    {
        juce::Path valArc;
        valArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f,
                             std::min(startAngle, angle), std::max(startAngle, angle), true);
        g.setColour(activeAccent.withAlpha(isHovered ? 1.0f : 0.88f));
        g.strokePath(valArc, juce::PathStrokeType(arcThick + 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Matte anodized studio dial body
    const float capRadius = radius * 0.74f;
    const auto capBounds = juce::Rectangle<float>(cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    juce::ColourGradient bodyGrad(Palette::panelElevated.brighter(0.06f), cx, cy - capRadius,
                                  Palette::controlWell, cx, cy + capRadius, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(capBounds);

    // Subtle machined rim
    g.setColour(isHovered ? activeAccent.withAlpha(0.45f) : Palette::borderFocus);
    g.drawEllipse(capBounds, 1.0f);

    // 4. Precision radial pointer indicator
    const float ptrOuter = capRadius - 3.5f;
    const float ptrInner = capRadius * 0.36f;
    const float sinA = std::sin(angle);
    const float cosA = -std::cos(angle);

    g.setColour(isEnabled ? Palette::textPrimary : Palette::textMuted);
    g.drawLine(cx + ptrInner * sinA, cy + ptrInner * cosA,
               cx + ptrOuter * sinA, cy + ptrOuter * cosA, 2.0f);
}

void ALRLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                          juce::Button& button,
                                          const juce::Colour& backgroundColour,
                                          bool isHighlighted,
                                          bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool toggled = button.getToggleState();

    auto fill = backgroundColour;
    if (!fill.isOpaque())
        fill = toggled ? Palette::accentGold.withAlpha(0.18f) : Palette::panelElevated;

    if (isDown)
        fill = fill.darker(0.15f);
    else if (isHighlighted)
        fill = fill.brighter(0.08f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 4.0f);

    const auto borderCol = toggled
                               ? button.findColour(juce::TextButton::buttonOnColourId).withAlpha(0.85f)
                               : (isHighlighted ? Palette::borderFocus : Palette::borderSubtle);
    g.setColour(borderCol.isTransparent() ? Palette::borderSubtle : borderCol);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

void ALRLookAndFeel::drawButtonText(juce::Graphics& g,
                                    juce::TextButton& button,
                                    bool, bool)
{
    const bool toggled = button.getToggleState();
    auto textCol = button.findColour(toggled ? juce::TextButton::textColourOnId
                                             : juce::TextButton::textColourOffId);
    if (!button.isEnabled())
        textCol = textCol.withAlpha(0.4f);

    g.setColour(textCol);
    g.setFont(makeFont(11.5f, true));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                     juce::Justification::centred, 1);
}

void ALRLookAndFeel::drawComboBox(juce::Graphics& g,
                                  int width, int height,
                                  bool isButtonDown,
                                  int, int, int, int,
                                  juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(isButtonDown ? Palette::panelElevated : Palette::controlWell);
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(box.isMouseOver() ? Palette::borderFocus : Palette::borderSubtle);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    // Crisp minimal chevron
    const float cx = static_cast<float>(width) - 13.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - 3.5f, cy - 1.8f);
    arrow.lineTo(cx, cy + 2.2f);
    arrow.lineTo(cx + 3.5f, cy - 1.8f);
    g.setColour(Palette::textSecondary);
    g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}

void ALRLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(Palette::panelElevated);
    g.setColour(Palette::borderFocus);
    g.drawRect(0, 0, width, height, 1);
}

void ALRLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isSeparator, bool isActive, bool isHighlighted,
                                       bool isTicked, bool,
                                       const juce::String& text,
                                       const juce::String&,
                                       const juce::Drawable*,
                                       const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(Palette::borderSubtle);
        g.fillRect(area.reduced(8, 0).withHeight(1).withCentre(area.getCentre()));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(Palette::accentGold.withAlpha(0.16f));
        g.fillRoundedRectangle(area.reduced(3, 1).toFloat(), 3.0f);
    }

    g.setFont(makeFont(12.0f, isTicked));
    g.setColour(isTicked ? Palette::accentGold : (isActive ? Palette::textPrimary : Palette::textMuted));
    g.drawFittedText(text, area.reduced(12, 0), juce::Justification::centredLeft, 1);
}

} // namespace alr::ui
