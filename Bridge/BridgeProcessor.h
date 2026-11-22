#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace solar {

class SolarBridgeProcessor : public juce::AudioProcessor {
public:
    SolarBridgeProcessor();
    ~SolarBridgeProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SolarStudio Bridge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    double getHostBpm() const { return hostBpm.load(); }
    double getHostPpq() const { return hostPpq.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    int getBridgeChannel() const { return bridgeChannel.load(); }
    void setBridgeChannel(int ch) { bridgeChannel.store(juce::jlimit(1, 16, ch)); }

private:
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostPpq { 0.0 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int> bridgeChannel { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarBridgeProcessor)
};

} // namespace solar
