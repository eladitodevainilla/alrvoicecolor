#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
// Original ALR Studio Palette — Charcoal, Graphite, Champagne Gold & Subtle Electric Blue
struct Palette
{
    static inline const juce::Colour bgMain        { 0xFF111215 };
    static inline const juce::Colour panelBg       { 0xFF17191E };
    static inline const juce::Colour panelElevated { 0xFF1E2128 };
    static inline const juce::Colour controlWell   { 0xFF13151A };
    static inline const juce::Colour borderSubtle  { 0xFF292D36 };
    static inline const juce::Colour borderFocus   { 0xFF3A404D };

    static inline const juce::Colour textPrimary   { 0xFFF2F4F8 };
    static inline const juce::Colour textSecondary { 0xFF969DAA };
    static inline const juce::Colour textMuted     { 0xFF5D6472 };

    static inline const juce::Colour accentGold    { 0xFFD2B478 };
    static inline const juce::Colour accentGoldDim { 0xFF7B6A49 };
    static inline const juce::Colour accentBlue    { 0xFF5A8ED6 };
    static inline const juce::Colour accentBlueDim { 0xFF34527A };

    static inline const juce::Colour statusGreen   { 0xFF4CC990 };
    static inline const juce::Colour statusAmber   { 0xFFE2A84B };
    static inline const juce::Colour statusRed     { 0xFFD85656 };
};

class ALRLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ALRLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override;

    static juce::Font makeFont(float heightPx, bool bold = false);
};

} // namespace alr::ui
