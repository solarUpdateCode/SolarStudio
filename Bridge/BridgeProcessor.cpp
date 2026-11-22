#include "BridgeProcessor.h"
#include "BridgeEditor.h"

namespace solar {

SolarBridgeProcessor::SolarBridgeProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void SolarBridgeProcessor::prepareToPlay(double, int) {}

bool SolarBridgeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SolarBridgeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm()) hostBpm.store(*bpm);
            if (auto ppq = pos->getPpqPosition()) hostPpq.store(*ppq);
            hostPlaying.store(pos->getIsPlaying());
        }
    }

    // Pass-through until SolarStudio standalone shared-memory stream attaches
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SolarBridgeProcessor::createEditor() {
    return new SolarBridgeEditor(*this);
}

void SolarBridgeProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream out(destData, true);
    out.writeInt(bridgeChannel.load());
}

void SolarBridgeProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream in(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= 4)
        bridgeChannel.store(juce::jlimit(1, 16, in.readInt()));
}

} // namespace solar

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new solar::SolarBridgeProcessor();
}
