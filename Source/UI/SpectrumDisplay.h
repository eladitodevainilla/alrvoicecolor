#pragma once

#include "../DSP/DSPCommon.h"
#include "../PluginProcessor.h"
#include "ALRLookAndFeel.h"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace alr::ui
{
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay(ALRVoiceColorAudioProcessor& processor);

    void updateFromAudioFifo();
    void setShowPreSpectrum(bool shouldShow) noexcept { showPre_ = shouldShow; }
    bool isShowingPreSpectrum() const noexcept { return showPre_; }

    void paint(juce::Graphics& g) override;

private:
    static constexpr std::size_t kFFTSize = 2048;
    static constexpr std::size_t kNumBins = kFFTSize / 2 + 1;

    float freqToX(float hz, juce::Rectangle<float> plot) const noexcept;
    float dbToY(float db, juce::Rectangle<float> plot) const noexcept;
    float eqDbToY(float db, juce::Rectangle<float> plot) const noexcept;

    ALRVoiceColorAudioProcessor& processor_;
    alr::dsp::Radix2FFT fft_;

    std::array<float, kFFTSize> preRing_{};
    std::array<float, kFFTSize> postRing_{};
    std::array<float, kFFTSize> workBuf_{};
    std::array<float, kNumBins> magScratch_{};
    std::array<float, kNumBins> smoothPreDb_{};
    std::array<float, kNumBins> smoothPostDb_{};
    std::array<float, 1024> popScratch_{};

    std::size_t preWrite_ = 0;
    std::size_t postWrite_ = 0;
    bool showPre_ = true;
};

} // namespace alr::ui
