#include "MeterComponent.h"
#include <algorithm>

namespace alr::ui
{

void MeterComponent::setLevels(float inPeakDb, float inRmsDb, float outPeakDb, float outRmsDb, float grDb) noexcept
{
    auto smoothMeter = [](float current, float target) noexcept
    {
        return (target > current) ? (current + 0.65f * (target - current))
                                  : (current + 0.16f * (target - current));
    };

    inPeakDb_  = smoothMeter(inPeakDb_, std::clamp(inPeakDb, -60.0f, 3.0f));
    inRmsDb_   = smoothMeter(inRmsDb_, std::clamp(inRmsDb, -60.0f, 3.0f));
    outPeakDb_ = smoothMeter(outPeakDb_, std::clamp(outPeakDb, -60.0f, 3.0f));
    outRmsDb_  = smoothMeter(outRmsDb_, std::clamp(outRmsDb, -60.0f, 3.0f));
    grDb_      = smoothMeter(grDb_, std::clamp(grDb, -18.0f, 0.0f));

    inHoldDb_  = std::max(inPeakDb_, inHoldDb_ - 0.35f);
    outHoldDb_ = std::max(outPeakDb_, outHoldDb_ - 0.35f);
}

float MeterComponent::dbToNorm(float db) const noexcept
{
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
}

void MeterComponent::drawVerticalMeter(juce::Graphics& g, juce::Rectangle<float> r,
                                       float peakDb, float rmsDb, float peakHoldDb,
                                       const juce::String& label, juce::Colour accent) const
{
    auto labelArea = r.removeFromTop(15.0f);
    g.setFont(ALRLookAndFeel::makeFont(9.5f, true));
    g.setColour(Palette::textSecondary);
    g.drawFittedText(label, labelArea.toNearestInt(), juce::Justification::centred, 1);

    auto readoutArea = r.removeFromBottom(14.0f);
    const juce::String dbStr = (peakDb <= -59.0f) ? "-inf" : juce::String(peakDb, 1);
    g.setFont(ALRLookAndFeel::makeFont(9.0f, false));
    g.setColour(peakDb > -0.5f ? Palette::statusRed : Palette::textMuted);
    g.drawFittedText(dbStr, readoutArea.toNearestInt(), juce::Justification::centred, 1);

    auto bar = r.reduced(3.0f, 2.0f);
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(bar, 2.5f);
    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(bar, 2.5f, 1.0f);

    // Peak bar (dimmer fill)
    const float peakH = bar.getHeight() * dbToNorm(peakDb);
    if (peakH > 1.0f)
    {
        auto pRect = bar.withTop(bar.getBottom() - peakH).reduced(1.0f, 0.5f);
        g.setColour(accent.withAlpha(0.32f));
        g.fillRoundedRectangle(pRect, 1.5f);
    }

    // RMS bar (solid primary)
    const float rmsH = bar.getHeight() * dbToNorm(rmsDb);
    if (rmsH > 1.0f)
    {
        auto rRect = bar.withTop(bar.getBottom() - rmsH).reduced(1.0f, 0.5f);
        auto barCol = (peakDb > -1.0f) ? Palette::statusRed
                    : (peakDb > -6.0f) ? Palette::statusAmber
                    : accent;
        g.setColour(barCol.withAlpha(0.88f));
        g.fillRoundedRectangle(rRect, 1.5f);
    }

    // Peak-hold hairline
    const float holdNorm = dbToNorm(peakHoldDb);
    if (holdNorm > 0.02f)
    {
        const float y = bar.getBottom() - bar.getHeight() * holdNorm;
        g.setColour(peakHoldDb > -1.0f ? Palette::statusRed : Palette::textPrimary.withAlpha(0.85f));
        g.fillRect(bar.getX() + 1.0f, y, bar.getWidth() - 2.0f, 1.5f);
    }
}

void MeterComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float colW = (w - 22.0f) * 0.5f;

    auto inRect  = area.removeFromLeft(colW);
    auto grRect  = area.removeFromLeft(22.0f);
    auto outRect = area;

    drawVerticalMeter(g, inRect, inPeakDb_, inRmsDb_, inHoldDb_, "IN", Palette::accentBlue);
    drawVerticalMeter(g, outRect, outPeakDb_, outRmsDb_, outHoldDb_, "OUT", Palette::accentGold);

    // Center Gain Reduction (GR) top-down meter
    auto grLabel = grRect.removeFromTop(15.0f);
    g.setFont(ALRLookAndFeel::makeFont(8.5f, true));
    g.setColour(Palette::textMuted);
    g.drawFittedText("GR", grLabel.toNearestInt(), juce::Justification::centred, 1);

    grRect.removeFromBottom(14.0f);
    auto grBar = grRect.reduced(5.0f, 2.0f);
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(grBar, 2.0f);

    const float grNorm = std::clamp(-grDb_ / 12.0f, 0.0f, 1.0f);
    if (grNorm > 0.01f)
    {
        g.setColour(Palette::statusAmber.withAlpha(0.85f));
        g.fillRoundedRectangle(grBar.withHeight(grBar.getHeight() * grNorm), 1.5f);
    }
}

} // namespace alr::ui
