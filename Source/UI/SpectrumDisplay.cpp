#include "SpectrumDisplay.h"
#include <algorithm>
#include <cmath>

namespace alr::ui
{

SpectrumDisplay::SpectrumDisplay(ALRVoiceColorAudioProcessor& processor)
    : processor_(processor)
{
    fft_.prepare(kFFTSize);
    smoothPreDb_.fill(-78.0f);
    smoothPostDb_.fill(-78.0f);
}

void SpectrumDisplay::updateFromAudioFifo()
{
    auto drainFifo = [&](auto popFn, std::array<float, kFFTSize>& ring, std::size_t& wPos,
                         std::array<float, kNumBins>& smoothDb)
    {
        bool gotData = false;
        std::size_t n = 0;
        while ((n = popFn(popScratch_.data(), popScratch_.size())) > 0)
        {
            gotData = true;
            for (std::size_t i = 0; i < n; ++i)
            {
                ring[wPos] = popScratch_[i];
                wPos = (wPos + 1) & (kFFTSize - 1);
            }
        }

        if (gotData)
        {
            for (std::size_t k = 0; k < kFFTSize; ++k)
                workBuf_[k] = ring[(wPos + k) & (kFFTSize - 1)];

            fft_.computeMagnitudeSpectrum(workBuf_.data(), magScratch_.data());
            for (std::size_t b = 0; b < kNumBins; ++b)
            {
                const float db = std::clamp(alr::dsp::gainToDb(magScratch_[b]), -78.0f, 0.0f);
                const float alpha = (db > smoothDb[b]) ? 0.45f : 0.16f;
                smoothDb[b] += alpha * (db - smoothDb[b]);
            }
        }
        else
        {
            // Gentle decay to floor when transport is stopped
            for (auto& v : smoothDb)
                v += 0.10f * (-78.0f - v);
        }
    };

    drainFifo([this](float* d, std::size_t c) { return processor_.popPreSpectrumSamples(d, c); },
              preRing_, preWrite_, smoothPreDb_);

    drainFifo([this](float* d, std::size_t c) { return processor_.popPostSpectrumSamples(d, c); },
              postRing_, postWrite_, smoothPostDb_);
}

float SpectrumDisplay::freqToX(float hz, juce::Rectangle<float> plot) const noexcept
{
    constexpr float fMin = 40.0f;
    constexpr float fMax = 18000.0f;
    const float norm = std::log10(std::clamp(hz, fMin, fMax) / fMin) / std::log10(fMax / fMin);
    return plot.getX() + norm * plot.getWidth();
}

float SpectrumDisplay::dbToY(float db, juce::Rectangle<float> plot) const noexcept
{
    const float norm = std::clamp((db + 72.0f) / 72.0f, 0.0f, 1.0f);
    return plot.getBottom() - norm * plot.getHeight();
}

float SpectrumDisplay::eqDbToY(float db, juce::Rectangle<float> plot) const noexcept
{
    // +/- 12 dB centered cleanly in plot
    const float norm = std::clamp((db + 12.0f) / 24.0f, 0.0f, 1.0f);
    return plot.getBottom() - norm * plot.getHeight();
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto plot = bounds.reduced(10.0f, 12.0f).withTrimmedBottom(10.0f);

    // 1. Minimal Studio Frequency & dB Grid
    const float gridFreqs[] = { 60.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    const char* gridLabels[] = { "60", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };

    g.setFont(ALRLookAndFeel::makeFont(9.0f, false));
    for (int i = 0; i < 9; ++i)
    {
        const float x = freqToX(gridFreqs[i], plot);
        g.setColour(Palette::borderSubtle.withAlpha(0.55f));
        g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());

        g.setColour(Palette::textMuted);
        g.drawFittedText(gridLabels[i], static_cast<int>(x) - 16, static_cast<int>(plot.getBottom()) + 1,
                         32, 12, juce::Justification::centred, 1);
    }

    // Center 0 dB EQ reference line
    const float zeroEqY = eqDbToY(0.0f, plot);
    g.setColour(Palette::borderFocus.withAlpha(0.65f));
    g.drawHorizontalLine(static_cast<int>(zeroEqY), plot.getX(), plot.getRight());

    const double sr = std::max(8000.0, processor_.getCurrentSampleRate());

    auto buildSpectrumPath = [&](const std::array<float, kNumBins>& specDb, bool closeForFill)
    {
        juce::Path p;
        const int numPts = 128;
        for (int i = 0; i < numPts; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numPts - 1);
            const float hz = 40.0f * std::pow(18000.0f / 40.0f, t);
            const float exactBin = hz * static_cast<float>(kFFTSize) / static_cast<float>(sr);
            const std::size_t b0 = static_cast<std::size_t>(std::clamp(std::floor(exactBin), 1.0f, static_cast<float>(kNumBins - 2)));
            const float frac = exactBin - static_cast<float>(b0);
            const float db = specDb[b0] + frac * (specDb[b0 + 1] - specDb[b0]);

            const float x = plot.getX() + t * plot.getWidth();
            const float y = dbToY(db, plot);

            if (i == 0) p.startNewSubPath(x, y);
            else        p.lineTo(x, y);
        }
        if (closeForFill)
        {
            p.lineTo(plot.getRight(), plot.getBottom());
            p.lineTo(plot.getX(), plot.getBottom());
            p.closeSubPath();
        }
        return p;
    };

    g.saveState();
    g.reduceClipRegion(plot.toNearestInt());

    // 2. Pre-Processing Spectrum (Subtle blue-grey reference fill)
    if (showPre_)
    {
        auto preFill = buildSpectrumPath(smoothPreDb_, true);
        g.setColour(Palette::accentBlue.withAlpha(0.11f));
        g.fillPath(preFill);

        auto preStroke = buildSpectrumPath(smoothPreDb_, false);
        g.setColour(Palette::accentBlue.withAlpha(0.35f));
        g.strokePath(preStroke, juce::PathStrokeType(1.0f));
    }

    // 3. Post-Processing Spectrum (Crisp off-white trace)
    {
        auto postFill = buildSpectrumPath(smoothPostDb_, true);
        g.setColour(Palette::textPrimary.withAlpha(0.08f));
        g.fillPath(postFill);

        auto postStroke = buildSpectrumPath(smoothPostDb_, false);
        g.setColour(Palette::textPrimary.withAlpha(0.65f));
        g.strokePath(postStroke, juce::PathStrokeType(1.25f));
    }

    // 4. Combined Voice Color + Learned Profile EQ Curve (Champagne Gold signature curve)
    {
        juce::Path eqCurve;
        const int numPts = 140;
        for (int i = 0; i < numPts; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numPts - 1);
            const float hz = 40.0f * std::pow(18000.0f / 40.0f, t);
            const float magDb = processor_.getEQMagnitudeDb(hz);
            const float x = plot.getX() + t * plot.getWidth();
            const float y = eqDbToY(magDb, plot);

            if (i == 0) eqCurve.startNewSubPath(x, y);
            else        eqCurve.lineTo(x, y);
        }

        // Subtle fill between curve and 0 dB line
        juce::Path eqFill = eqCurve;
        eqFill.lineTo(plot.getRight(), zeroEqY);
        eqFill.lineTo(plot.getX(), zeroEqY);
        eqFill.closeSubPath();
        g.setColour(Palette::accentGold.withAlpha(0.11f));
        g.fillPath(eqFill);

        g.setColour(Palette::accentGold);
        g.strokePath(eqCurve, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.restoreState();

    // Top-left subtle legend
    g.setFont(ALRLookAndFeel::makeFont(9.5f, true));
    g.setColour(Palette::textMuted);
    g.drawText("SPECTRAL & TONAL RESPONSE", static_cast<int>(plot.getX()) + 4, static_cast<int>(bounds.getY()) + 3,
               200, 14, juce::Justification::centredLeft);

    g.setFont(ALRLookAndFeel::makeFont(9.0f, false));
    g.setColour(Palette::accentGold);
    g.drawText("EQ / PROFILE", static_cast<int>(plot.getRight()) - 150, static_cast<int>(bounds.getY()) + 3,
               74, 14, juce::Justification::centredRight);
    g.setColour(Palette::textSecondary);
    g.drawText("POST", static_cast<int>(plot.getRight()) - 68, static_cast<int>(bounds.getY()) + 3,
               32, 14, juce::Justification::centredRight);
    g.setColour(Palette::accentBlue.withAlpha(0.75f));
    g.drawText("PRE", static_cast<int>(plot.getRight()) - 32, static_cast<int>(bounds.getY()) + 3,
               28, 14, juce::Justification::centredRight);
}

} // namespace alr::ui
