#include "RelearnPanel.h"
#include <algorithm>
#include <cmath>

namespace alr::ui
{

RelearnPanel::RelearnPanel(ALRVoiceColorAudioProcessor& processor)
    : processor_(processor)
{
    relearnBtn_.setColour(juce::TextButton::buttonColourId, Palette::panelElevated);
    relearnBtn_.setColour(juce::TextButton::buttonOnColourId, Palette::accentGold);
    relearnBtn_.setColour(juce::TextButton::textColourOffId, Palette::textPrimary);
    relearnBtn_.setColour(juce::TextButton::textColourOnId, Palette::accentGold);
    relearnBtn_.onClick = [this]()
    {
        if (telemetry_.isRelearning)
            processor_.triggerStopRelearn();
        else
            processor_.triggerStartRelearn();
    };
    addAndMakeVisible(relearnBtn_);

    resetBtn_.setColour(juce::TextButton::buttonColourId, Palette::controlWell);
    resetBtn_.setColour(juce::TextButton::textColourOffId, Palette::textSecondary);
    resetBtn_.onClick = [this]() { processor_.triggerResetProfile(); };
    addAndMakeVisible(resetBtn_);
}

void RelearnPanel::updateTelemetry(const alr::dsp::ChainTelemetry& telemetry, const alr::dsp::VoiceProfile& profile)
{
    telemetry_ = telemetry;
    profile_ = profile;
    if (telemetry_.isRelearning)
    {
        pulsePhase_ += 0.14f;
        relearnBtn_.setToggleState(true, juce::dontSendNotification);
        relearnBtn_.setButtonText(telemetry_.relearnActiveSec >= 2.0f ? "STOP & APPLY" : "LISTENING...");
    }
    else
    {
        pulsePhase_ = 0.0f;
        relearnBtn_.setToggleState(false, juce::dontSendNotification);
        relearnBtn_.setButtonText(profile_.isValid ? "RELEARN VOICE" : "RELEARN");
    }
    resetBtn_.setEnabled(profile_.isValid || telemetry_.isRelearning);
}

void RelearnPanel::resized()
{
    auto area = getLocalBounds().reduced(14);
    area.removeFromTop(24); // Section title
    relearnBtn_.setBounds(area.removeFromTop(38));
    resetBtn_.setBounds(area.removeFromBottom(28));
}

void RelearnPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto inner = getLocalBounds().reduced(14);

    // Header
    g.setFont(ALRLookAndFeel::makeFont(10.5f, true));
    g.setColour(Palette::accentGold);
    g.drawText("VOICE ANALYSIS", inner.removeFromTop(18), juce::Justification::centredLeft);
    inner.removeFromTop(6);
    inner.removeFromTop(38); // Space for RELEARN button
    inner.removeFromTop(14);

    // Progress Bar + 40s Active Timer
    auto progArea = inner.removeFromTop(34);
    g.setFont(ALRLookAndFeel::makeFont(10.0f, true));
    g.setColour(Palette::textSecondary);
    g.drawText("ACTIVE VOICE CAPTURED", progArea.withHeight(14), juce::Justification::centredLeft);

    const juce::String timeStr = juce::String(telemetry_.relearnActiveSec, 1) + "s / 40.0s";
    g.setColour(telemetry_.isRelearning ? Palette::accentGold : Palette::textPrimary);
    g.drawText(timeStr, progArea.withHeight(14), juce::Justification::centredRight);

    auto barBg = progArea.withTrimmedTop(20).withHeight(7).toFloat();
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(barBg, 3.5f);

    const float fillW = barBg.getWidth() * std::clamp(telemetry_.relearnProgress, 0.0f, 1.0f);
    if (fillW > 1.0f)
    {
        g.setColour(telemetry_.isRelearning ? Palette::accentGold : Palette::statusGreen);
        g.fillRoundedRectangle(barBg.withWidth(fillW), 3.5f);
    }

    inner.removeFromTop(14);

    // Live VAD Status Card
    auto vadCard = inner.removeFromTop(54).toFloat();
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(vadCard, 5.0f);
    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(vadCard, 5.0f, 1.0f);

    juce::String vadText = "SILENCE (PAUSED)";
    juce::Colour vadCol  = Palette::textMuted;
    switch (telemetry_.vad.state)
    {
        case alr::dsp::VoiceState::ActiveVoice:
            vadText = telemetry_.vad.isSustainedVowel ? "ACTIVE VOICE (VOWEL)" : "ACTIVE VOICE";
            vadCol  = Palette::statusGreen;
            break;
        case alr::dsp::VoiceState::UnvoicedConsonant:
            vadText = "CONSONANT / SIBILANT";
            vadCol  = Palette::accentBlue;
            break;
        case alr::dsp::VoiceState::Breath:
            vadText = "BREATH (IGNORED)";
            vadCol  = Palette::statusAmber;
            break;
        case alr::dsp::VoiceState::BackgroundNoise:
            vadText = "ROOM NOISE (IGNORED)";
            vadCol  = Palette::textMuted;
            break;
        case alr::dsp::VoiceState::Silence:
        default:
            break;
    }

    g.setFont(ALRLookAndFeel::makeFont(9.0f, true));
    g.setColour(Palette::textMuted);
    g.drawText("DETECTOR STATE", vadCard.reduced(10, 7).withHeight(12).toNearestInt(), juce::Justification::centredLeft);

    const float dotAlpha = telemetry_.isRelearning ? (0.55f + 0.45f * std::sin(pulsePhase_)) : 0.9f;
    g.setColour(vadCol.withAlpha(dotAlpha));
    g.fillEllipse(vadCard.getX() + 10.0f, vadCard.getY() + 28.0f, 8.0f, 8.0f);

    g.setFont(ALRLookAndFeel::makeFont(11.0f, true));
    g.setColour(Palette::textPrimary);
    g.drawText(vadText, static_cast<int>(vadCard.getX()) + 24, static_cast<int>(vadCard.getY()) + 24,
               static_cast<int>(vadCard.getWidth()) - 32, 16, juce::Justification::centredLeft);

    inner.removeFromTop(14);
    inner.removeFromBottom(36); // Reserve for RESET button

    // Learned Vocal Profile Telemetry Card
    auto profCard = inner.toFloat();
    g.setColour(Palette::controlWell);
    g.fillRoundedRectangle(profCard, 5.0f);
    g.setColour(profile_.isValid ? Palette::accentGold.withAlpha(0.45f) : Palette::borderSubtle);
    g.drawRoundedRectangle(profCard, 5.0f, 1.0f);

    auto pIn = profCard.reduced(10, 9).toNearestInt();
    g.setFont(ALRLookAndFeel::makeFont(9.5f, true));
    g.setColour(profile_.isValid ? Palette::accentGold : Palette::textMuted);
    g.drawText(profile_.isValid ? "PROFILE LEARNED" : "NO PROFILE LEARNED", pIn.removeFromTop(16), juce::Justification::centredLeft);

    auto drawRow = [&](const juce::String& k, const juce::String& v, juce::Colour vCol = Palette::textPrimary)
    {
        auto row = pIn.removeFromTop(21);
        g.setFont(ALRLookAndFeel::makeFont(10.0f, false));
        g.setColour(Palette::textSecondary);
        g.drawText(k, row, juce::Justification::centredLeft);
        g.setFont(ALRLookAndFeel::makeFont(10.5f, true));
        g.setColour(vCol);
        g.drawText(v, row, juce::Justification::centredRight);
    };

    pIn.removeFromTop(4);
    if (profile_.isValid)
    {
        drawRow("Median F0",      juce::String(static_cast<int>(std::round(profile_.medianF0Hz))) + " Hz");
        drawRow("HPF Anchor",     juce::String(static_cast<int>(std::round(profile_.targets.hpfCutoffHz))) + " Hz");
        drawRow("Body Focus",     juce::String(static_cast<int>(std::round(profile_.targets.bodyFreqHz))) + " Hz");
        drawRow("Dynamic Spread", juce::String(profile_.dynamicRangeDb, 1) + " dB");
        drawRow("Harmonic Index", juce::String(static_cast<int>(std::round(profile_.harmonicDensity * 100.0f))) + "%");
        drawRow("Top Resonance",  juce::String(static_cast<int>(std::round(profile_.resonanceFreqHz[1]))) + " Hz", Palette::accentGold);
    }
    else
    {
        g.setFont(ALRLookAndFeel::makeFont(10.5f, false));
        g.setColour(Palette::textMuted);
        g.drawFittedText("Press RELEARN and play up to 40s of vocal audio. Silences and breaths are ignored automatically.",
                         pIn.reduced(0, 6), juce::Justification::topLeft, 5);
    }
}

} // namespace alr::ui
