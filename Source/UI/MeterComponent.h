#pragma once

#include "ALRLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class MeterComponent : public juce::Component
{
public:
    void setLevels(float inPeakDb, float inRmsDb, float outPeakDb, float outRmsDb, float grDb) noexcept;
    void paint(juce::Graphics& g) override;

private:
    float dbToNorm(float db) const noexcept;
    void drawVerticalMeter(juce::Graphics& g, juce::Rectangle<float> r,
                           float peakDb, float rmsDb, float peakHoldDb,
                           const juce::String& label, juce::Colour accent) const;

    float inPeakDb_ = -60.0f, inRmsDb_ = -60.0f, inHoldDb_ = -60.0f;
    float outPeakDb_ = -60.0f, outRmsDb_ = -60.0f, outHoldDb_ = -60.0f;
    float grDb_ = 0.0f;
};

} // namespace alr::ui
