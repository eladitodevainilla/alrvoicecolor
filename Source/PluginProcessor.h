#pragma once

#include "DSP/VocalProcessingChain.h"
#include "Parameters/ParameterManager.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class ALRVoiceColorAudioProcessor : public juce::AudioProcessor
{
public:
    ALRVoiceColorAudioProcessor();
    ~ALRVoiceColorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ALR VOICE COLOR"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default Studio Vocal"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }

    // UI Controls (lock-free / atomic dispatch)
    void triggerStartRelearn() noexcept { chain_.startRelearn(); }
    void triggerStopRelearn() noexcept { chain_.stopRelearn(); }
    void triggerResetProfile() noexcept { chain_.resetProfile(); }

    alr::dsp::VoiceProfile getVoiceProfile() const noexcept { return chain_.getVoiceProfile(); }
    alr::dsp::ChainTelemetry getLatestTelemetry() const noexcept;
    float getEQMagnitudeDb(float freqHz) const noexcept { return chain_.getEQMagnitudeDb(freqHz); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate_.load(std::memory_order_relaxed); }

    std::size_t popPreSpectrumSamples(float* dest, std::size_t maxCount) noexcept
    {
        return chain_.popPreSpectrumSamples(dest, maxCount);
    }

    std::size_t popPostSpectrumSamples(float* dest, std::size_t maxCount) noexcept
    {
        return chain_.popPostSpectrumSamples(dest, maxCount);
    }

private:
    juce::AudioProcessorValueTreeState apvts_;
    alr::dsp::VocalProcessingChain chain_;

    // Double-buffered lock-free telemetry handoff (Audio thread -> GUI thread)
    alr::dsp::ChainTelemetry telemetrySlots_[2]{};
    std::atomic<int> telemetryReadSlot_{0};
    std::atomic<double> currentSampleRate_{48000.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ALRVoiceColorAudioProcessor)
};
