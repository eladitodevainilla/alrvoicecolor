#include "PluginEditor.h"
#include "Parameters/ParameterIDs.h"
#include <algorithm>

ALRVoiceColorAudioProcessorEditor::ALRVoiceColorAudioProcessorEditor(ALRVoiceColorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      relearnPanel_(p),
      spectrumDisplay_(p),
      voiceColorPanel_(p.getAPVTS()),
      pitchPanel_(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel_);

    // Header buttons
    autoGainBtn_.setClickingTogglesState(true);
    autoGainBtn_.setColour(juce::TextButton::buttonColourId, alr::ui::Palette::controlWell);
    autoGainBtn_.setColour(juce::TextButton::buttonOnColourId, alr::ui::Palette::accentGold);
    autoGainBtn_.setColour(juce::TextButton::textColourOffId, alr::ui::Palette::textSecondary);
    autoGainBtn_.setColour(juce::TextButton::textColourOnId, alr::ui::Palette::accentGold);
    addAndMakeVisible(autoGainBtn_);

    settingsBtn_.setColour(juce::TextButton::buttonColourId, alr::ui::Palette::controlWell);
    settingsBtn_.setColour(juce::TextButton::textColourOffId, alr::ui::Palette::textSecondary);
    settingsBtn_.onClick = [this]() { openSettingsMenu(); };
    addAndMakeVisible(settingsBtn_);

    bypassBtn_.setClickingTogglesState(true);
    bypassBtn_.setColour(juce::TextButton::buttonColourId, alr::ui::Palette::controlWell);
    bypassBtn_.setColour(juce::TextButton::buttonOnColourId, alr::ui::Palette::statusRed);
    bypassBtn_.setColour(juce::TextButton::textColourOffId, alr::ui::Palette::textSecondary);
    bypassBtn_.setColour(juce::TextButton::textColourOnId, alr::ui::Palette::statusRed);
    addAndMakeVisible(bypassBtn_);

    autoGainAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.getAPVTS(), alr::params::autoGainComp, autoGainBtn_);
    bypassAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.getAPVTS(), alr::params::bypass, bypassBtn_);

    // Panels
    addAndMakeVisible(relearnPanel_);
    addAndMakeVisible(spectrumDisplay_);
    addAndMakeVisible(voiceColorPanel_);
    addAndMakeVisible(pitchPanel_);
    addAndMakeVisible(meterComponent_);

    outputGainKnob_ = std::make_unique<alr::ui::RotaryKnobComponent>(
        processor_.getAPVTS(), alr::params::outputGain, "OUT GAIN", alr::ui::Palette::accentGold, true);
    globalMixKnob_ = std::make_unique<alr::ui::RotaryKnobComponent>(
        processor_.getAPVTS(), alr::params::globalMix, "MIX", alr::ui::Palette::textPrimary, true);
    addAndMakeVisible(*outputGainKnob_);
    addAndMakeVisible(*globalMixKnob_);

    setResizable(true, true);
    setResizeLimits(980, 580, 1480, 860);
    setSize(1140, 660);

    // 30 Hz smooth UI refresh (low CPU overhead, zero audio thread blocking)
    startTimerHz(30);
}

ALRVoiceColorAudioProcessorEditor::~ALRVoiceColorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ALRVoiceColorAudioProcessorEditor::openSettingsMenu()
{
    juce::PopupMenu menu;
    const double sr = processor_.getCurrentSampleRate();
    const int lat   = processor_.getLatencySamples();
    const double latMs = (sr > 0.0) ? (1000.0 * static_cast<double>(lat) / sr) : 0.0;

    menu.addSectionHeader("ALR VOICE COLOR  |  ENGINE STATUS");
    menu.addItem(1, "Show Pre-Processing Spectrum", true, spectrumDisplay_.isShowingPreSpectrum());
    menu.addSeparator();
    menu.addItem(100, "Sample Rate: " + juce::String(static_cast<int>(sr)) + " Hz", false, false);
    menu.addItem(101, "Reported Latency: " + juce::String(lat) + " smp (" + juce::String(latMs, 1) + " ms)", false, false);
    menu.addItem(102, "Auto-Gain Comp: " + juce::String(latestTelemetry_.autoGainAppliedDb, 1) + " dB", false, false);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsBtn_),
                       [this](int result)
                       {
                           if (result == 1)
                               spectrumDisplay_.setShowPreSpectrum(!spectrumDisplay_.isShowingPreSpectrum());
                       });
}

void ALRVoiceColorAudioProcessorEditor::timerCallback()
{
    latestTelemetry_ = processor_.getLatestTelemetry();
    latestProfile_   = processor_.getVoiceProfile();

    spectrumDisplay_.updateFromAudioFifo();
    relearnPanel_.updateTelemetry(latestTelemetry_, latestProfile_);
    pitchPanel_.updateTelemetry(latestTelemetry_.pitch);
    meterComponent_.setLevels(latestTelemetry_.inputPeakDb,
                              latestTelemetry_.inputRmsDb,
                              latestTelemetry_.outputPeakDb,
                              latestTelemetry_.outputRmsDb,
                              latestTelemetry_.gainReductionDb);

    repaint();
}

void ALRVoiceColorAudioProcessorEditor::drawALRLogo(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Original minimalist vector monogram badge for ALR
    g.setColour(alr::ui::Palette::accentGold.withAlpha(0.14f));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(alr::ui::Palette::accentGold);
    g.drawRoundedRectangle(area, 4.0f, 1.2f);

    g.setFont(alr::ui::ALRLookAndFeel::makeFont(13.0f, true));
    g.setColour(alr::ui::Palette::accentGold);
    g.drawFittedText("ALR", area.toNearestInt(), juce::Justification::centred, 1);
}

void ALRVoiceColorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header Bar (52 px)
    auto header = bounds.removeFromTop(52).reduced(14, 11);
    bypassBtn_.setBounds(header.removeFromRight(76));
    header.removeFromRight(8);
    settingsBtn_.setBounds(header.removeFromRight(82));
    header.removeFromRight(8);
    autoGainBtn_.setBounds(header.removeFromRight(86));

    // Main Content Area
    auto content = bounds.reduced(12, 10);
    const int gap = 10;

    // Left Column: VOICE ANALYSIS (RELEARN)
    relearnPanel_.setBounds(content.removeFromLeft(248));
    content.removeFromLeft(gap);

    // Right Column: OUTPUT & METERING
    outputPanelBounds_ = content.removeFromRight(146);
    content.removeFromRight(gap);

    {
        auto outIn = outputPanelBounds_.reduced(10, 10);
        outIn.removeFromTop(20); // OUTPUT header
        auto knobBot = outIn.removeFromBottom(162);
        meterComponent_.setBounds(outIn.reduced(2, 4));
        outputGainKnob_->setBounds(knobBot.removeFromTop(80).reduced(6, 2));
        globalMixKnob_->setBounds(knobBot.reduced(6, 2));
    }

    // Center Column: Spectrum (top) + Voice Color (middle) + ALR Pitch (bottom)
    const int specH  = std::max(145, static_cast<int>(content.getHeight() * 0.30f));
    const int pitchH = std::max(138, static_cast<int>(content.getHeight() * 0.25f));

    spectrumDisplay_.setBounds(content.removeFromTop(specH));
    content.removeFromTop(gap);
    pitchPanel_.setBounds(content.removeFromBottom(pitchH));
    content.removeFromBottom(gap);
    voiceColorPanel_.setBounds(content);
}

void ALRVoiceColorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(alr::ui::Palette::bgMain);

    // Top Header Bar
    auto headerBar = getLocalBounds().removeFromTop(52);
    g.setColour(alr::ui::Palette::panelBg);
    g.fillRect(headerBar);
    g.setColour(alr::ui::Palette::borderSubtle);
    g.fillRect(0, 51, getWidth(), 1);

    // Logo & Product Title
    drawALRLogo(g, juce::Rectangle<float>(16.0f, 13.0f, 44.0f, 26.0f));

    g.setFont(alr::ui::ALRLookAndFeel::makeFont(16.0f, true));
    g.setColour(alr::ui::Palette::textPrimary);
    g.drawText("VOICE COLOR", 70, 12, 180, 28, juce::Justification::centredLeft);

    // Subtle activity status pill in header center
    const bool bypassed = bypassBtn_.getToggleState();
    const bool activeVoice = latestTelemetry_.vad.isActiveVocal && !bypassed;
    const juce::Rectangle<float> statusPill(static_cast<float>(getWidth()) * 0.5f - 108.0f, 14.0f, 216.0f, 24.0f);
    g.setColour(alr::ui::Palette::controlWell);
    g.fillRoundedRectangle(statusPill, 12.0f);
    g.setColour(alr::ui::Palette::borderSubtle);
    g.drawRoundedRectangle(statusPill, 12.0f, 1.0f);

    const auto dotCol = bypassed ? alr::ui::Palette::statusRed
                      : (latestTelemetry_.isRelearning ? alr::ui::Palette::accentGold
                      : (activeVoice ? alr::ui::Palette::statusGreen : alr::ui::Palette::textMuted));
    g.setColour(dotCol);
    g.fillEllipse(statusPill.getX() + 12.0f, statusPill.getCentreY() - 3.5f, 7.0f, 7.0f);

    juce::String statusLabel = bypassed ? "ENGINE BYPASSED"
                             : (latestTelemetry_.isRelearning ? "RELEARN ANALYZING..."
                             : (latestProfile_.isValid ? "ADAPTIVE PROFILE ACTIVE" : "STUDIO VOCAL ENGINE"));
    g.setFont(alr::ui::ALRLookAndFeel::makeFont(10.0f, true));
    g.setColour(alr::ui::Palette::textSecondary);
    g.drawText(statusLabel, statusPill.withTrimmedLeft(24.0f).toNearestInt(), juce::Justification::centredLeft);

    // Output Panel Background & Header
    if (!outputPanelBounds_.isEmpty())
    {
        const auto ob = outputPanelBounds_.toFloat();
        g.setColour(alr::ui::Palette::panelBg);
        g.fillRoundedRectangle(ob, 6.0f);
        g.setColour(alr::ui::Palette::borderSubtle);
        g.drawRoundedRectangle(ob.reduced(0.5f), 6.0f, 1.0f);

        g.setFont(alr::ui::ALRLookAndFeel::makeFont(10.5f, true));
        g.setColour(alr::ui::Palette::accentGold);
        g.drawText("OUTPUT", outputPanelBounds_.getX() + 10, outputPanelBounds_.getY() + 9,
                   outputPanelBounds_.getWidth() - 20, 16, juce::Justification::centred);
    }
}
