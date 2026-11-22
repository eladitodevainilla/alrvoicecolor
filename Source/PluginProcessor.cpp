#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <memory>

ALRVoiceColorAudioProcessor::ALRVoiceColorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "ALRVoiceColorState", alr::ParameterManager::createParameterLayout())
{
}

ALRVoiceColorAudioProcessor::~ALRVoiceColorAudioProcessor() = default;

bool ALRVoiceColorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Allow Mono->Mono, Stereo->Stereo, and Mono->Stereo
    if (mainIn != mainOut && !(mainIn == juce::AudioChannelSet::mono() && mainOut == juce::AudioChannelSet::stereo()))
        return false;

    return true;
}

void ALRVoiceColorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate_.store(sampleRate, std::memory_order_relaxed);
    chain_.prepare(sampleRate, std::max(64, samplesPerBlock));
    setLatencySamples(chain_.getLatencySamples());
}

void ALRVoiceColorAudioProcessor::releaseResources()
{
    chain_.reset();
}

void ALRVoiceColorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels  = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples        = buffer.getNumSamples();

    for (int ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (numSamples <= 0 || numOutputChannels == 0)
        return;

    // Mono-to-stereo copy before DSP processing if DAW feeds mono track into stereo bus
    if (numInputChannels == 1 && numOutputChannels >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    const auto params = alr::ParameterManager::readChainParameters(apvts_);
    chain_.setParameters(params);

    float* left  = buffer.getWritePointer(0);
    float* right = (numOutputChannels >= 2) ? buffer.getWritePointer(1) : nullptr;

    chain_.processBlock(left, right, numSamples);

    // Publish telemetry snapshot lock-free for GUI thread
    const int writeSlot = 1 - telemetryReadSlot_.load(std::memory_order_relaxed);
    telemetrySlots_[writeSlot] = chain_.getTelemetry();
    telemetryReadSlot_.store(writeSlot, std::memory_order_release);
}

alr::dsp::ChainTelemetry ALRVoiceColorAudioProcessor::getLatestTelemetry() const noexcept
{
    return telemetrySlots_[telemetryReadSlot_.load(std::memory_order_acquire)];
}

void ALRVoiceColorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();

    // Remove any stale embedded VoiceProfile child and embed the current learned VoiceProfile
    auto existingProfile = state.getChildWithName("ALRVoiceProfile");
    if (existingProfile.isValid())
        state.removeChild(existingProfile, nullptr);

    state.appendChild(alr::ParameterManager::serializeVoiceProfile(chain_.getVoiceProfile()), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void ALRVoiceColorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    if (xmlState->hasTagName(apvts_.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        auto profileTree = tree.getChildWithName("ALRVoiceProfile");
        if (profileTree.isValid())
        {
            const auto restoredProfile = alr::ParameterManager::deserializeVoiceProfile(profileTree);
            chain_.setVoiceProfile(restoredProfile);
            tree.removeChild(profileTree, nullptr);
        }
        apvts_.replaceState(tree);
    }
}

juce::AudioProcessorEditor* ALRVoiceColorAudioProcessor::createEditor()
{
    return new ALRVoiceColorAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ALRVoiceColorAudioProcessor();
}
